#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// ─── Application IDs (big-endian u32 at AMIIBO_OFF_APPID) ────────────────────
// Source: community reverse-engineering (yellows8, Tagmo, GBAtemp threads).

/** Super Smash Bros. Ultimate (Nintendo Switch) */
#define SSBU_APP_ID          0xD188918FU

/** Super Smash Bros. for Wii U (USA/EUR) */
#define SSB4U_APP_ID         0x100B0400U

/** Super Smash Bros. for Wii U (Japan) */
#define SSB4U_JP_APP_ID      0x10110E00U

/** Super Smash Bros. for Nintendo 3DS */
#define SSB4_3DS_APP_ID      0x10116E00U

// ─── Game identifier enum ─────────────────────────────────────────────────────

/**
 * Which Smash game initialized this amiibo's figure-player block.
 * Detected from the Application ID stored in the tag header.
 */
typedef enum {
    SmashGameUnknown = 0,   ///< Not a recognized Smash Bros. tag
    SmashGameSSBU,          ///< Super Smash Bros. Ultimate (Switch)
    SmashGameSSB4U,         ///< Super Smash Bros. for Wii U
    SmashGameSSB43DS,       ///< Super Smash Bros. for Nintendo 3DS
} SmashGame;

// ─── Figure Player block offsets (within decrypted game data, from
//     dump byte AMIIBO_USER_OFFSET + AMIIBO_OFF_GAME_DATA = dump[0xDC + 16])
//     Offsets are identical across SSBU, SSB4-U, and SSB4-3DS for core stats.
//     Verified against community reverse-engineering of all three save blocks.
// ─────────────────────────────────────────────────────────────────────────────

#define SSBU_OFF_LEVEL      0x00   // uint8  — Amiibo level 1–50
#define SSBU_OFF_ATK        0x02   // int16 BE — attack bonus  (–5000..+5000)
#define SSBU_OFF_DEF        0x04   // int16 BE — defense bonus
#define SSBU_OFF_SPD        0x06   // int16 BE — speed bonus
#define SSBU_OFF_PERSONA    0x08   // uint8  — AI personality archetype (0–6)
#define SSBU_OFF_SPIRIT1    0x0A   // uint8  — spirit slot 1 modifier (SSBU only)
#define SSBU_OFF_SPIRIT2    0x0B   // uint8  — spirit slot 2 modifier (SSBU only)
#define SSBU_OFF_SPIRIT3    0x0C   // uint8  — spirit slot 3 modifier (SSBU only)
#define SSBU_OFF_WEIGHTS    0x20   // int16[64] BE — move priority table (SSBU only)

#define SSBU_WEIGHT_COUNT   64

// ─── Clamp ranges ─────────────────────────────────────────────────────────────

#define SSBU_LEVEL_MIN   1
#define SSBU_LEVEL_MAX   50
#define SSBU_STAT_MIN    (-5000)
#define SSBU_STAT_MAX    5000
#define SSBU_WEIGHT_MIN  (-32768)
#define SSBU_WEIGHT_MAX  32767

// ─── Personality presets ──────────────────────────────────────────────────────

/** Named AI personalities stored in the personality byte (same across all three games) */
typedef enum {
    SsbuPersonaNormal     = 0,
    SsbuPersonaAggressive = 1,
    SsbuPersonaCalm       = 2,
    SsbuPersonaCareless   = 3,
    SsbuPersonaCautious   = 4,
    SsbuPersonaTricky     = 5,
    SsbuPersonaJoyful     = 6,
    SsbuPersonaCount      = 7,
} SsbuPersona;

/** Human-readable names for the personality presets */
extern const char* const SSBU_PERSONA_NAMES[SsbuPersonaCount];

// ─── Move name labels for the weight table ───────────────────────────────────

/**
 * Approximate move index → label mapping (SSBU only).
 */
const char* ssbu_weight_label(uint8_t index);

// ─── Main struct ──────────────────────────────────────────────────────────────

/**
 * In-memory representation of a parsed Smash Bros. Figure Player.
 * All values are in host byte order; serialisation handles endianness.
 *
 * The 'game' field identifies which title initialized the tag and controls
 * which fields are valid/editable.
 */
typedef struct {
    SmashGame game;                         // which game owns this tag
    uint8_t  level;                         // 1–50
    int16_t  attack;                        // stat bonus
    int16_t  defense;
    int16_t  speed;
    uint8_t  persona;                       // SsbuPersona enum value
    uint8_t  spirit[3];                     // spirit modifier bytes (SSBU only)
    int16_t  weights[SSBU_WEIGHT_COUNT];    // move priority table (SSBU only)
    bool     dirty;                         // true when unsaved changes exist
} SmashFigure;

// ─── Multi-game API ───────────────────────────────────────────────────────────

/**
 * Detect which Smash Bros. game initialized this tag by reading its App ID.
 * @param dump  Raw 540-byte dump (game area must be decrypted).
 * @return      SmashGame enum value; SmashGameUnknown if not a Smash tag.
 */
SmashGame smash_detect_game(const uint8_t* dump);

/**
 * Return a short human-readable name for the detected game, suitable for
 * display on the Flipper's 128×64 screen.
 * Examples: "SSB Ultimate", "SSB Wii-U", "SSB 3DS"
 */
const char* smash_game_name(SmashGame game);

/**
 * Returns true if the Move Weights editor is supported for this game.
 * Currently only SSBU has a fully-documented weight table — SSB4 tags
 * use a different (less-documented) format, so weights are hidden for them.
 */
bool smash_is_weights_supported(SmashGame game);

/**
 * Parse Smash figure-player data from a decrypted dump.
 * Handles SSBU, SSB4-U, and SSB4-3DS.  Core stats (Level/ATK/DEF/SPD/Persona)
 * are always parsed.  Weights and Spirits are parsed only for SSBU.
 *
 * @param dump  540-byte dump (game area must be decrypted).
 * @param out   Destination SmashFigure.
 * @return      true on success; false if not any recognised Smash tag.
 */
bool smash_parse(const uint8_t* dump, SmashFigure* out);

/** Backward-compatible alias — same as smash_parse() but returns false for non-SSBU. */
bool ssbu_is_smash_tag(const uint8_t* dump);

/**
 * Parse the SSBU game data block from an (already decrypted) dump.
 * @deprecated  Use smash_parse() for multi-game support.
 */
bool ssbu_parse(const uint8_t* dump, SmashFigure* out);

/**
 * Write a SmashFigure back into the dump's game data block.
 * Works for all three Smash games (same stat offsets).
 * Call amiibo_crypto_encrypt() afterwards to re-sign the tag.
 * @param figure  Source SmashFigure (values will be clamped).
 * @param dump    540-byte dump (modified in place).
 */
void ssbu_serialize(const SmashFigure* figure, uint8_t* dump);

/** Clamp all stat values to their valid ranges. */
void ssbu_clamp(SmashFigure* figure);

/** Reset move weights to the default neutral distribution (all 0). SSBU only. */
void ssbu_reset_weights(SmashFigure* figure);
