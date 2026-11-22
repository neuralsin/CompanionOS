#ifndef COMPANION_THOUGHT_PERSONALITY_H
#define COMPANION_THOUGHT_PERSONALITY_H
#include <stdint.h>
#include <stddef.h>
namespace thought_personality {
static uint8_t flirtLevel = 1;
static uint32_t sequence = 0;
static char context[80] = {};
static const char* const playful[] = {
  "You look good with a plan. Just saying.", "My favorite view? You showing up.",
  "One more chapter, then you get my full attention.", "That focused look suits you a little too well.",
  "Your playlist is cute. Its owner is trouble.", "Are you always this distracting?",
  "You bring the charm. I'll keep the time.", "You make ordinary evenings interesting.",
  "My circuits have excellent taste. Exhibit A: you.", "Consider this your very subtle desk-side wink.",
  "You, me, and a finished to-do list. A good date.", "I'd blush, but the display budget says no."
};
static const char* const bold[] = {
  "Come a little closer. That smile is distracting.", "You make this desk look dangerously good.",
  "Careful with that look. I'm trying to behave.", "Finish that chapter. Then we can flirt properly.",
  "If chemistry were an exam, we'd be trouble.", "You have my attention. All of it.",
  "That confidence? Unfairly attractive.", "I had a clever line. Then you smiled.",
  "A little tension looks good on us.", "Keep looking at me like that. See what happens.",
  "You're making it very hard to play it cool.", "Tonight's forecast: undeniable chemistry."
};
static const char* const focus[] = {
  "Pick the smallest next step. Start there.", "A finished page beats a perfect plan.",
  "Water, shoulders down, one clear task.", "Try recalling it before checking your notes.",
  "Write the question you keep avoiding.", "A five-minute start still counts.",
  "Leave one useful note for future you.", "Progress doesn't need an audience.",
  "One tab. One task. A little breathing room.", "Practice the hard question while energy is high.",
  "A break is part of the plan, not a failure.", "You don't have to solve the whole week today."
};
inline const char* choose(bool hasTime, int hour, bool playing, bool weatherValid,
                          int temperature, unsigned pick) {
  ++sequence;
  if (flirtLevel && sequence % 3 == 0)
    return flirtLevel > 1 ? bold[pick % 12] : playful[pick % 12];
  if (context[0] && sequence % 3 == 1) return context;
  if (hasTime && (hour >= 23 || hour < 5)) {
    static const char* const night[] = {"Late night. Save your progress and rest.",
      "Tomorrow's focus starts with tonight's sleep.", "Quiet hours. One gentle landing, then bed."};
    return night[pick % 3];
  }
  if (hasTime && pick % 3 == 0) {
    if (hour < 12) return "Morning energy: give the hardest task first choice.";
    if (hour < 17) return "Afternoon reset: water, a stretch, then one small win.";
    return "Evening check-in: what can wait until tomorrow?";
  }
  if (playing && pick % 3 == 1) return "Good soundtrack. Let's give it a good study session.";
  if (weatherValid && pick % 4 == 0) {
    if (temperature >= 32) return "Warm out. Keep some water within reach.";
    if (temperature <= 14) return "Chilly out. A warm drink might help.";
  }
  return focus[pick % 12];
}
// The built-in bitmap font has no UTF-8 decoder. Never paint an empty bubble.
inline void sanitize(char* text, size_t capacity) {
  size_t out = 0;
  bool space = true;
  for (size_t i = 0; i + 1 < capacity && text[i]; ++i) {
    unsigned char ch = text[i];
    if (ch < 33 || ch > 126) {
      if (!space && out + 1 < capacity) text[out++] = ' ';
      space = true;
    } else { text[out++] = char(ch); space = false; }
  }
  while (out && text[out - 1] == ' ') --out;
  text[out] = 0;
}
}
#endif
