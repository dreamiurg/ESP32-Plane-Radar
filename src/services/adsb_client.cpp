#include "services/adsb_client.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include <ArduinoJson.h>

#include <cctype>
#include <cmath>
#include <cstring>

#include "config.h"

namespace services::adsb {

namespace {

constexpr char kApiBase[] = "https://opendata.adsb.fi/api/v3/lat/";
constexpr char kRouteApiBase[] = "https://api.adsbdb.com/v0/callsign/";
constexpr float kKmPerNm = 1.852f;
constexpr int kConnectTimeoutMs = 5000;  // TLS handshake needs room
constexpr int kConnectAttempts = 1;  // a stalled TLS connect blocks the UI; retry next poll instead
constexpr unsigned long kRequestTimeoutMs = 6000;
unsigned long s_last_success_ms = 0;

Aircraft s_aircraft[kMaxAircraft];
size_t s_aircraft_count = 0;
PollFn s_poll_fn = nullptr;

void pollNetwork() {
  if (s_poll_fn != nullptr) {
    s_poll_fn();
  }
}

float kmToNauticalMiles(float km) { return km / kKmPerNm; }

bool readJsonFloat(const JsonObject& obj, const char* key, float* out) {
  if (obj[key].is<float>() || obj[key].is<double>() || obj[key].is<int>()) {
    *out = obj[key].as<float>();
    return true;
  }
  return false;
}

float pickNoseHeading(const JsonObject& plane) {
  float v = 0.0f;
  if (readJsonFloat(plane, "true_heading", &v)) {
    return v;
  }
  if (readJsonFloat(plane, "mag_heading", &v)) {
    return v;
  }
  if (readJsonFloat(plane, "track", &v)) {
    return v;
  }
  if (readJsonFloat(plane, "dir", &v)) {
    return v;
  }
  return 0.0f;
}

float pickTrackHeading(const JsonObject& plane) {
  float v = 0.0f;
  if (readJsonFloat(plane, "track", &v)) {
    return v;
  }
  if (readJsonFloat(plane, "true_heading", &v)) {
    return v;
  }
  if (readJsonFloat(plane, "mag_heading", &v)) {
    return v;
  }
  if (readJsonFloat(plane, "dir", &v)) {
    return v;
  }
  return 0.0f;
}

float pickGroundSpeed(const JsonObject& plane) {
  float v = 0.0f;
  if (readJsonFloat(plane, "gs", &v)) {
    return v;
  }
  if (readJsonFloat(plane, "tas", &v)) {
    return v;
  }
  if (readJsonFloat(plane, "ias", &v)) {
    return v;
  }
  return 0.0f;
}

bool isOnGround(const JsonObject& plane) {
  if (!plane["alt_baro"].is<const char*>()) {
    return false;
  }
  return strcmp(plane["alt_baro"].as<const char*>(), "ground") == 0;
}

void copyJsonStringTrimmed(const JsonObject& obj, const char* key, char* out,
                           size_t out_len) {
  out[0] = '\0';
  if (out_len == 0 || !obj[key].is<const char*>()) {
    return;
  }
  const char* s = obj[key].as<const char*>();
  size_t n = strnlen(s, out_len - 1);
  while (n > 0 && s[n - 1] == ' ') {
    --n;
  }
  memcpy(out, s, n);
  out[n] = '\0';
}

void formatAltitudeTag(const JsonObject& plane, char* out, size_t out_len) {
  out[0] = '\0';
  if (out_len == 0) {
    return;
  }

  if (plane["alt_baro"].is<const char*>()) {
    const char* s = plane["alt_baro"].as<const char*>();
    if (strcmp(s, "ground") == 0) {
      strncpy(out, "GND", out_len - 1);
      out[out_len - 1] = '\0';
      return;
    }
  }

  float alt = 0.0f;
  if (readJsonFloat(plane, "alt_baro", &alt) ||
      readJsonFloat(plane, "alt_geom", &alt)) {
    snprintf(out, out_len, "%d ft", static_cast<int>(lroundf(alt)));
  }
}

bool httpGetJson(const String& url, const char* tag, JsonDocument& doc,
                 const JsonDocument& filter, int* status_out = nullptr) {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  if (!http.begin(client, url)) {
    Serial.printf("%s: http.begin failed\n", tag);
    return false;
  }

  http.useHTTP10(true);
  http.setTimeout(kRequestTimeoutMs);
  http.setConnectTimeout(kConnectTimeoutMs);

  int code = 0;
  for (int attempt = 0; attempt < kConnectAttempts; ++attempt) {
    pollNetwork();
    code = http.GET();
    if (code > 0) {
      break;
    }
  }
  if (status_out != nullptr) {
    *status_out = code;
  }
  if (code != HTTP_CODE_OK) {
    Serial.printf("%s: HTTP %d\n", tag, code);
    http.end();
    return false;
  }

  const DeserializationError err = deserializeJson(
      doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err) {
    Serial.printf("%s: JSON parse error: %s\n", tag, err.c_str());
    return false;
  }
  return true;
}

// --- Route lookup (adsbdb.com), cached per callsign ---
// ponytail: fixed 32-entry round-robin cache; grow if you watch a busy hub.
struct RouteEntry {
  char callsign[9];
  char route[8];
};
constexpr size_t kRouteCacheSize = 32;
RouteEntry s_route_cache[kRouteCacheSize] = {};
size_t s_route_cache_next = 0;

const RouteEntry* findRoute(const char* callsign) {
  for (const RouteEntry& e : s_route_cache) {
    if (e.callsign[0] != '\0' && strcmp(e.callsign, callsign) == 0) {
      return &e;
    }
  }
  return nullptr;
}

constexpr float kEarthRadiusKm = 6371.0f;
constexpr float kDegToRad = 3.14159265f / 180.0f;
/** Max distance from the origin->destination great circle to trust a route.
 *  Airlines reuse flight numbers; a stale DB entry puts the plane far off. */
constexpr float kRouteMaxCrossTrackKm = 200.0f;

float centralAngle(float la1, float lo1, float la2, float lo2) {
  const float c = sinf(la1) * sinf(la2) + cosf(la1) * cosf(la2) * cosf(lo2 - lo1);
  return acosf(fmaxf(-1.0f, fminf(1.0f, c)));
}

float bearing(float la1, float lo1, float la2, float lo2) {
  return atan2f(sinf(lo2 - lo1) * cosf(la2),
                cosf(la1) * sinf(la2) - sinf(la1) * cosf(la2) * cosf(lo2 - lo1));
}

/** Plane at (plat,plon) is near the great circle from A to B and not past it. */
bool routePlausible(float plat, float plon, float alat, float alon, float blat,
                    float blon) {
  plat *= kDegToRad; plon *= kDegToRad;
  alat *= kDegToRad; alon *= kDegToRad;
  blat *= kDegToRad; blon *= kDegToRad;
  const float d_ap = centralAngle(alat, alon, plat, plon);
  const float d_ab = centralAngle(alat, alon, blat, blon);
  const float xt = asinf(sinf(d_ap) * sinf(bearing(alat, alon, plat, plon) -
                                           bearing(alat, alon, blat, blon)));
  const float max_rad = kRouteMaxCrossTrackKm / kEarthRadiusKm;
  return fabsf(xt) <= max_rad && d_ap <= d_ab + max_rad;
}

/** Query adsbdb once and cache the result (empty route on miss or when the
 *  plane's position doesn't fit the route). */
void lookupRoute(const char* callsign, float plat, float plon, char* out,
                 size_t out_len) {
  out[0] = '\0';
  JsonDocument filter;
  JsonObject ap = filter["response"]["flightroute"]["origin"].to<JsonObject>();
  ap["iata_code"] = true;
  ap["latitude"] = true;
  ap["longitude"] = true;
  filter["response"]["flightroute"]["destination"] = ap;

  JsonDocument doc;
  int status = 0;
  if (!httpGetJson(String(kRouteApiBase) + callsign, "route", doc, filter,
                   &status)) {
    if (status != HTTP_CODE_NOT_FOUND) {
      return;  // transport/server error: don't cache, retry next cycle
    }
    // 404 = adsbdb knows no route for this callsign; cache the miss below.
  }

  JsonObject fr = doc["response"]["flightroute"].as<JsonObject>();
  JsonObject o = fr["origin"].as<JsonObject>();
  JsonObject d = fr["destination"].as<JsonObject>();
  const char* oc = o["iata_code"].as<const char*>();
  const char* dc = d["iata_code"].as<const char*>();
  if (oc != nullptr && dc != nullptr) {
    // Missing airport coordinates: accept rather than score against (0,0).
    const bool have_coords =
        o["latitude"].is<float>() && o["longitude"].is<float>() &&
        d["latitude"].is<float>() && d["longitude"].is<float>();
    if (!have_coords ||
        routePlausible(plat, plon, o["latitude"].as<float>(),
                       o["longitude"].as<float>(), d["latitude"].as<float>(),
                       d["longitude"].as<float>())) {
      snprintf(out, out_len, "%s-%s", oc, dc);
    } else {
      Serial.printf("route: %s %s-%s rejected, plane off track\n", callsign, oc,
                    dc);
    }
  }
  RouteEntry& e = s_route_cache[s_route_cache_next];
  s_route_cache_next = (s_route_cache_next + 1) % kRouteCacheSize;
  strncpy(e.callsign, callsign, sizeof(e.callsign) - 1);
  e.callsign[sizeof(e.callsign) - 1] = '\0';
  strncpy(e.route, out, sizeof(e.route) - 1);
  e.route[sizeof(e.route) - 1] = '\0';
  Serial.printf("route: %s -> %s\n", callsign, out[0] ? out : "(none)");
}

/** Fill routes from cache; at most one network lookup per call. */
void fillRoutes(Aircraft* list, size_t n) {
  bool looked_up = false;
  for (size_t i = 0; i < n; ++i) {
    Aircraft& ac = list[i];
    ac.route[0] = '\0';
    if (ac.callsign[0] == '\0') {
      continue;
    }
    const RouteEntry* e = findRoute(ac.callsign);
    if (e != nullptr) {
      strncpy(ac.route, e->route, sizeof(ac.route) - 1);
      ac.route[sizeof(ac.route) - 1] = '\0';
    } else if (!looked_up) {
      looked_up = true;
      lookupRoute(ac.callsign, ac.lat, ac.lon, ac.route, sizeof(ac.route));
    }
  }
}

/** Private/general aviation: ADS-B emitter category A1 (light) or A2 (small);
 *  if no category, an N-number callsign (N + digit) counts as private. */
bool isPrivateAircraft(const JsonObject& plane, const char* callsign) {
  if (plane["category"].is<const char*>()) {
    const char* c = plane["category"].as<const char*>();
    return strcmp(c, "A1") == 0 || strcmp(c, "A2") == 0;
  }
  return callsign[0] == 'N' && isdigit(static_cast<unsigned char>(callsign[1]));
}

void fillTagFields(Aircraft* ac, const JsonObject& plane) {
  copyJsonStringTrimmed(plane, "flight", ac->callsign, sizeof(ac->callsign));
  if (ac->callsign[0] == '\0') {
    copyJsonStringTrimmed(plane, "hex", ac->callsign, sizeof(ac->callsign));
  }

  copyJsonStringTrimmed(plane, "t", ac->type, sizeof(ac->type));
  formatAltitudeTag(plane, ac->alt, sizeof(ac->alt));
  ac->is_private = isPrivateAircraft(plane, ac->callsign);
}

/** Parse the aircraft list into s_aircraft; the JsonDocument dies on return. */
bool fetchAircraftInto(double center_lat, double center_lon,
                       float fetch_radius_km, size_t* out_count) {
  const float dist_nm = kmToNauticalMiles(fetch_radius_km);

  String url = kApiBase;
  url += String(center_lat, 6);
  url += "/lon/";
  url += String(center_lon, 6);
  url += "/dist/";
  url += String(dist_nm, 1);

  JsonDocument filter;
  JsonObject f = filter["ac"].add<JsonObject>();
  for (const char* key :
       {"lat", "lon", "true_heading", "mag_heading", "track", "dir", "gs",
        "tas", "ias", "alt_baro", "alt_geom", "flight", "hex", "t",
        "category"}) {
    f[key] = true;
  }

  JsonDocument doc;
  if (!httpGetJson(url, "adsb", doc, filter)) {
    return false;
  }

  *out_count = 0;
  JsonArray ac = doc["ac"].as<JsonArray>();
  if (ac.isNull()) {
    return true;
  }

  size_t n = 0;
  for (JsonObject plane : ac) {
    if (n >= kMaxAircraft) {
      break;
    }
    if (!plane["lat"].is<float>() || !plane["lon"].is<float>()) {
      continue;
    }
    if (isOnGround(plane) && !config::kAdsbShowGroundAircraft) {
      continue;
    }

    s_aircraft[n].lat = plane["lat"].as<float>();
    s_aircraft[n].lon = plane["lon"].as<float>();
    s_aircraft[n].nose_deg = pickNoseHeading(plane);
    s_aircraft[n].track_deg = pickTrackHeading(plane);
    s_aircraft[n].gs_knots = pickGroundSpeed(plane);
    fillTagFields(&s_aircraft[n], plane);
    ++n;
  }
  *out_count = n;
  return true;
}

}  // namespace

void setPollFn(PollFn fn) { s_poll_fn = fn; }

size_t aircraftCount() { return s_aircraft_count; }

const Aircraft* aircraftList() { return s_aircraft; }

bool fetchUpdate(double center_lat, double center_lon, float fetch_radius_km) {
  size_t n = 0;
  if (!fetchAircraftInto(center_lat, center_lon, fetch_radius_km, &n)) {
    return false;
  }
  s_aircraft_count = n;
  s_last_success_ms = millis();
  // The aircraft JsonDocument is gone by now: only one TLS session + document
  // is alive while routes are fetched.
  fillRoutes(s_aircraft, n);
  Serial.printf("adsb: %u aircraft\n", static_cast<unsigned>(n));
  return true;
}

unsigned long lastSuccessMs() { return s_last_success_ms; }

}  // namespace services::adsb
