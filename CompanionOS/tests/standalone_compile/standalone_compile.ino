// Compile-only, secret-free integration harness. Do not upload for live testing.
#include <Arduino.h>
#include <WiFiUdp.h>
#define UDP_PORT_TX 4211
WiFiUDP udp;
String pcIPStr;
#ifndef ENABLE_SPOTIFY
#define ENABLE_SPOTIFY 1
#endif
#define WEATHER_LATITUDE 0.0
#define WEATHER_LONGITUDE 0.0
#define WEATHER_CITY_NAME "Test"
#define WEATHER_POLL_INTERVAL_MS 900000UL
#define SPOTIFY_POLL_INTERVAL_ACTIVE_MS 3500UL
#define SPOTIFY_POLL_INTERVAL_IDLE_MS 10000UL
#define SPOTIFY_POLL_INTERVAL_LYRICS_MS 1000UL
#define ALBUM_ART_H 64
enum State { STATE_EYES, STATE_SPOTIFY, STATE_LYRICS, STATE_WEATHER, STATE_CLOCK_DASHBOARD, STATE_RETRO_WATCH, STATE_COUNT };
State currentState = STATE_EYES;
bool wifiConnected = false, dhNetPaused = false, isHostAndroid = false;
bool isPlaying = false, musicPlaying = false, albumArtReady = false, receivingArt = false, artDrawn = false;
int activeTheme = 0, playProgress = 0, playDuration = 0;
String spotifyAccessToken, spotifyRefreshToken, spotifyClientId, spotifyClientSecret;
String currentTrack, currentArtist, currentLyrics, currentLyricsLine2, prevLyricsLine;
String weatherCondition, weatherCity, weatherSunrise, weatherSunset;
int weatherTemp, weatherFeels, weatherHumidity, weatherWind, weatherHigh, weatherLow, weatherCode;
uint16_t* albumArt = nullptr;
uint16_t* albumArtIncoming = nullptr;
uint16_t* galleryImage = nullptr;
uint8_t albumArtRowsReceived[64], albumArtRowsComplete = 0;
uint32_t albumArtRevision = 0;
unsigned long albumArtStartedAt = 0, spotifyProgressUpdatedAt = 0;
bool t2_shuffle = false;
uint8_t t2_volume = 100, t2_repeat = 0;
char t2_device[24] = {};
#ifndef STANDALONE_HOST_TEST
bool isHostActive() { return false; }
bool isHostPlaybackActive() { return false; }
bool isHostWeatherActive() { return false; }
#endif
String encodeBase64(const String&) { return ""; }
void redrawSpotifyPartial() {}
void redrawWeatherPartial() {}
void redrawClockDashboardPartial() {}
void redrawRetroWatchPartial() {}
void t2_redrawSpotifyPartial() {}
void t2_resetSpotifyDrawState() {}
void resetSpotifyDrawState() {}
bool albumArtPageNearby() { return currentState == STATE_EYES || currentState == STATE_SPOTIFY || currentState == STATE_LYRICS; }
void prepareStandaloneMemory(bool) {}
#include "standalone_services.h"
void setup() { tickStandaloneServices(); queueStandaloneCommand("NEXT"); }
void loop() {}
