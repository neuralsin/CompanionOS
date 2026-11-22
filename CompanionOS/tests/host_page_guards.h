#ifndef COMPANION_TEST_HOST_PAGE_GUARDS_H
#define COMPANION_TEST_HOST_PAGE_GUARDS_H

// Only unrelated hardware pages/animation engines are replaced. ui.h,
// theme2_spotify.h and pages.h below are included unmodified and executed.
#define EYES_H
#define THEME2_EYES_H
void drawStarfield() {}
void drawEyes() {}
void t2_drawEyesPage() {}
void t3_drawEyesPage() {}
void releaseEyeStrips() {}
void renderCurrentPage();

// NVS/HTTP are hardware boundaries; planner rendering uses its real source.
#define COMPANION_PLANNER_STORE_H
#include "../arduino/CompanionOS_Main/planner_core.h"
#include "../arduino/CompanionOS_Main/thought_personality.h"
#include <ctime>
namespace companion_planner {
static Plan plan;
static uint32_t revision=0, alertStarted=0, lastAlert=0;
static bool& alertVisible=overlayActive;
static bool alertRed=false;
static char alertText[80]={};
inline bool dismissAlerts() { bool active=alertVisible; alertVisible=false; return active; }
}
inline bool dismissThought() { bool active=activeBubble.active; activeBubble.active=false; return active; }

#endif
