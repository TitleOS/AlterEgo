#include "amiibo_smash.h"
#include "amiibo_crypto.h"

#include <string.h>
#include <furi.h>

#define TAG "SmashFigure"

// ─── Personality name table ───────────────────────────────────────────────────

const char* const SSBU_PERSONA_NAMES[SsbuPersonaCount] = {
    "Normal",
    "Aggressive",
    "Calm",
    "Careless",
    "Cautious",
    "Tricky",
    "Joyful",
};

// ─── Move label table ─────────────────────────────────────────────────────────
// Known SSBU move weight indices (community documented; unknown = "Move %02d")

static const char* const WEIGHT_LABELS[SSBU_WEIGHT_COUNT] = {
    /* 00 */ "Jab",
    /* 01 */ "Rapid Jab",
    /* 02 */ "Dash Attack",
    /* 03 */ "Side Tilt",
    /* 04 */ "Up Tilt",
    /* 05 */ "Down Tilt",
    /* 06 */ "Side Smash",
    /* 07 */ "Up Smash",
    /* 08 */ "Down Smash",
    /* 09 */ "N-Air",
    /* 10 */ "F-Air",
    /* 11 */ "B-Air",
    /* 12 */ "U-Air",
    /* 13 */ "D-Air",
    /* 14 */ "Neutral B",
    /* 15 */ "Side B",
    /* 16 */ "Up B",
    /* 17 */ "Down B",
    /* 18 */ "Grab",
    /* 19 */ "Pummel",
    /* 20 */ "F-Throw",
    /* 21 */ "B-Throw",
    /* 22 */ "U-Throw",
    /* 23 */ "D-Throw",
    /* 24 */ "Roll Fwd",
    /* 25 */ "Roll Back",
    /* 26 */ "Spot Dodge",
    /* 27 */ "Air Dodge",
    /* 28 */ "Jump",
    /* 29 */ "Double Jump",
    /* 30 */ "Shield",
    /* 31 */ "Ledge Attack",
    /* 32 */ "Ledge Jump",
    /* 33 */ "Ledge Roll",
    /* 34 */ "Short Hop",
    /* 35 */ "getup Atk",
    /* 36 */ "Charge Hold",
    /* 37 */ "Charge Drop",
    /* 38 */ "Tech in Place",
    /* 39 */ "Tech Roll Fwd",
    /* 40 */ "Tech Roll Back",
    /* 41 */ "Miss Tech",
    /* 42 */ "Edge Hang",
    /* 43 */ "Footstool",
    /* 44 */ "Move 44",
    /* 45 */ "Move 45",
    /* 46 */ "Move 46",
    /* 47 */ "Move 47",
    /* 48 */ "Move 48",
    /* 49 */ "Move 49",
    /* 50 */ "Move 50",
    /* 51 */ "Move 51",
    /* 52 */ "Move 52",
    /* 53 */ "Move 53",
    /* 54 */ "Move 54",
    /* 55 */ "Move 55",
    /* 56 */ "Move 56",
    /* 57 */ "Move 57",
    /* 58 */ "Move 58",
    /* 59 */ "Move 59",
    /* 60 */ "Move 60",
    /* 61 */ "Move 61",
    /* 62 */ "Move 62",
    /* 63 */ "Move 63",
};

const char* ssbu_weight_label(uint8_t index) {
    if(index >= SSBU_WEIGHT_COUNT) return "???";
    return WEIGHT_LABELS[index];
}

// ─── Helpers ──────────────────────────────────────────────────────────────────

static inline int16_t read_be16(const uint8_t* p) {
    return (int16_t)((uint16_t)p[0] << 8 | p[1]);
}

static inline void write_be16(uint8_t* p, int16_t v) {
    p[0] = (uint8_t)((uint16_t)v >> 8);
    p[1] = (uint8_t)(v & 0xFF);
}

static inline int16_t clamp16(int16_t v, int16_t lo, int16_t hi) {
    if(v < lo) return lo;
    if(v > hi) return hi;
    return v;
}

// ─── Pointer to game data in dump ────────────────────────────────────────────

static const uint8_t* game_data_ro(const uint8_t* dump) {
    return dump + AMIIBO_USER_OFFSET + AMIIBO_OFF_GAME_DATA;
}

static uint8_t* game_data_rw(uint8_t* dump) {
    return dump + AMIIBO_USER_OFFSET + AMIIBO_OFF_GAME_DATA;
}

// ─── Read raw App ID from dump ────────────────────────────────────────────────

static uint32_t read_app_id(const uint8_t* dump) {
    const uint8_t* p = dump + AMIIBO_USER_OFFSET + AMIIBO_OFF_APPID;
    return (uint32_t)p[0] << 24 |
           (uint32_t)p[1] << 16 |
           (uint32_t)p[2] << 8  |
                      p[3];
}

// ─── Multi-game detection ─────────────────────────────────────────────────────

SmashGame smash_detect_game(const uint8_t* dump) {
    furi_assert(dump);

    const uint8_t* p = dump + AMIIBO_USER_OFFSET + AMIIBO_OFF_APPID;
    uint32_t app_id  = (uint32_t)p[0] << 24 |
                       (uint32_t)p[1] << 16 |
                       (uint32_t)p[2] << 8  |
                                  p[3];

    // Dump settings area (0x028–0x037) for diagnostic purposes
    const uint8_t* s = dump + AMIIBO_USER_OFFSET + 0x028;
    FURI_LOG_I(TAG,
               "AppID @ dump[%d]: %02X %02X %02X %02X = 0x%08lX",
               (int)(AMIIBO_USER_OFFSET + AMIIBO_OFF_APPID),
               p[0], p[1], p[2], p[3], (unsigned long)app_id);
    FURI_LOG_D(TAG,
               "Settings[0x28..0x37]: "
               "%02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
               s[0], s[1], s[2],  s[3],  s[4],  s[5],  s[6],  s[7],
               s[8], s[9], s[10], s[11], s[12], s[13], s[14], s[15]);

    switch(app_id) {
    case SSBU_APP_ID:          return SmashGameSSBU;
    case SSB4U_APP_ID:         return SmashGameSSB4U;
    case SSB4U_JP_APP_ID:      return SmashGameSSB4U;
    case SSB4_3DS_APP_ID:      return SmashGameSSB43DS;
    default:
        FURI_LOG_W(TAG, "Unknown AppID 0x%08lX", (unsigned long)app_id);
        return SmashGameUnknown;
    }
}

const char* smash_game_name(SmashGame game) {
    switch(game) {
    case SmashGameSSBU:    return "SSB Ultimate";
    case SmashGameSSB4U:   return "SSB Wii-U";
    case SmashGameSSB43DS: return "SSB 3DS";
    default:               return "Unknown";
    }
}

bool smash_is_weights_supported(SmashGame game) {
    // Move weights are only supported for SSBU — SSB4 uses a different
    // (less-documented) format at the same region and editing it blindly
    // could corrupt the tag.
    return game == SmashGameSSBU;
}

// ─── Backward-compat helper ───────────────────────────────────────────────────

bool ssbu_is_smash_tag(const uint8_t* dump) {
    return smash_detect_game(dump) == SmashGameSSBU;
}

// ─── Parse (multi-game) ───────────────────────────────────────────────────────

bool smash_parse(const uint8_t* dump, SmashFigure* out) {
    furi_assert(dump);
    furi_assert(out);

    SmashGame game = smash_detect_game(dump);
    if(game == SmashGameUnknown) {
        FURI_LOG_W(TAG, "AppID 0x%08lX — not a Smash Bros. tag",
                   (unsigned long)read_app_id(dump));
        return false;
    }

    out->game = game;

    const uint8_t* gd = game_data_ro(dump);

    // Core stats — identical offsets across SSBU, SSB4-U, SSB4-3DS
    out->level   = gd[SSBU_OFF_LEVEL];
    out->attack  = read_be16(gd + SSBU_OFF_ATK);
    out->defense = read_be16(gd + SSBU_OFF_DEF);
    out->speed   = read_be16(gd + SSBU_OFF_SPD);
    out->persona = gd[SSBU_OFF_PERSONA];

    // SSBU-exclusive fields
    if(game == SmashGameSSBU) {
        out->spirit[0] = gd[SSBU_OFF_SPIRIT1];
        out->spirit[1] = gd[SSBU_OFF_SPIRIT2];
        out->spirit[2] = gd[SSBU_OFF_SPIRIT3];

        for(uint8_t i = 0; i < SSBU_WEIGHT_COUNT; i++)
            out->weights[i] = read_be16(gd + SSBU_OFF_WEIGHTS + i * 2);
    } else {
        // Clear SSBU-only fields so they are always in a defined state
        out->spirit[0] = out->spirit[1] = out->spirit[2] = 0;
        for(uint8_t i = 0; i < SSBU_WEIGHT_COUNT; i++)
            out->weights[i] = 0;
    }

    // Make sure values won't crash the editor UI arrays
    ssbu_clamp(out);
    out->dirty = false;

    FURI_LOG_I(TAG, "Parsed [%s]: Lv%u ATK:%d DEF:%d SPD:%d persona:%u",
               smash_game_name(game),
               out->level, out->attack, out->defense, out->speed, out->persona);
    return true;
}

// ─── Backward-compat parse (SSBU only) ───────────────────────────────────────

bool ssbu_parse(const uint8_t* dump, SmashFigure* out) {
    // Delegates to smash_parse; returns false if not an SSBU tag
    if(!smash_parse(dump, out)) return false;
    if(out->game != SmashGameSSBU) {
        FURI_LOG_W(TAG, "ssbu_parse: tag is %s, not SSBU",
                   smash_game_name(out->game));
        return false;
    }
    return true;
}

// ─── Serialize ────────────────────────────────────────────────────────────────

void ssbu_serialize(const SmashFigure* figure, uint8_t* dump) {
    furi_assert(figure);
    furi_assert(dump);

    // Write a clamped copy
    SmashFigure clamped = *figure;
    ssbu_clamp(&clamped);

    uint8_t* gd = game_data_rw(dump);

    // Core stats — same offsets for all three games
    gd[SSBU_OFF_LEVEL]   = (uint8_t)clamped.level;
    write_be16(gd + SSBU_OFF_ATK, clamped.attack);
    write_be16(gd + SSBU_OFF_DEF, clamped.defense);
    write_be16(gd + SSBU_OFF_SPD, clamped.speed);
    gd[SSBU_OFF_PERSONA]  = clamped.persona;

    // SSBU-exclusive fields
    if(clamped.game == SmashGameSSBU) {
        gd[SSBU_OFF_SPIRIT1]  = clamped.spirit[0];
        gd[SSBU_OFF_SPIRIT2]  = clamped.spirit[1];
        gd[SSBU_OFF_SPIRIT3]  = clamped.spirit[2];

        for(uint8_t i = 0; i < SSBU_WEIGHT_COUNT; i++)
            write_be16(gd + SSBU_OFF_WEIGHTS + i * 2, clamped.weights[i]);
    }
}

// ─── Clamp ────────────────────────────────────────────────────────────────────

void ssbu_clamp(SmashFigure* figure) {
    if(figure->level < SSBU_LEVEL_MIN) figure->level = SSBU_LEVEL_MIN;
    if(figure->level > SSBU_LEVEL_MAX) figure->level = SSBU_LEVEL_MAX;

    figure->attack  = clamp16(figure->attack,  SSBU_STAT_MIN, SSBU_STAT_MAX);
    figure->defense = clamp16(figure->defense, SSBU_STAT_MIN, SSBU_STAT_MAX);
    figure->speed   = clamp16(figure->speed,   SSBU_STAT_MIN, SSBU_STAT_MAX);

    if(figure->persona >= SsbuPersonaCount) figure->persona = SsbuPersonaNormal;

    for(uint8_t i = 0; i < SSBU_WEIGHT_COUNT; i++)
        figure->weights[i] = clamp16(figure->weights[i],
                                     SSBU_WEIGHT_MIN, SSBU_WEIGHT_MAX);
}

void ssbu_reset_weights(SmashFigure* figure) {
    memset(figure->weights, 0, sizeof(figure->weights));
    figure->dirty = true;
}
