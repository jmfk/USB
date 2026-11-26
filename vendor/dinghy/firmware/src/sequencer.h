/* SPDX-License-Identifier: GPL-3.0-only */
/* FM-1 sequencer experiment. No flash writes; see firmware-test/README.md. */
#define SQ_PAGES 4u
#define SQ_STEPS (16u * SQ_PAGES)
#define SQ_NOTES 11u
#define SQ_TAP 300u
#define SQ_HOLD 180u
#define SQ_ACC RGB(180, 140, 255)
#define SQ_DIM RGB(130, 130, 130)
static const uint8_t sq_white[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
static const uint8_t sq_black[11] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25};
typedef struct
{
    uint8_t used, pitch, velocity, probability, gate;
} sq_note_t;
typedef struct
{
    uint8_t enabled, length;
    sq_note_t note[SQ_NOTES];
} sq_step_t;
static sq_step_t sq_steps[SQ_STEPS];
static uint32_t sq_page, sq_pages = 1, sq_step, sq_slot, sq_bpm = 120, sq_playing, sq_head;
static uint32_t sq_next, sq_rem, sq_seq_count, sq_rand = 1234567;
static uint8_t sq_seq_notes[NV * 16u], sq_refs[128];
static int32_t sq_edit = -1, sq_latch = -1, sq_temp = -1, sq_anchor = -1, sq_oct;
static uint32_t sq_shift_down, sq_shift_latch, sq_record, sq_cursor, sq_shift_tap, sq_shift_at;
static uint32_t sq_prev, sq_down_at[27], sq_used[27], sq_tap_at[16], sq_tap_pending[16], sq_tap_step[16],
    sq_tap_before[16];
static uint32_t sq_step_notes_before[27], sq_tap_notes_before[16];
static uint32_t sq_before[27], sq_parent_before[27], sq_note_step[27];
static uint32_t sq_note_tap[11], sq_note_tap_at[11], sq_note_tap_step[11], sq_note_tap_before[11],
    sq_note_tap_parent[11];
static int32_t sq_note_latch = -1, sq_note_latch_step = -1;
static uint32_t sq_menu, sq_menu_page[6], sq_bank;
static int32_t sq_origin[27]; /* -2 unused, -1 slot, >=0 step, >=128 musical pitch */
static uint8_t sq_live[128], sq_capture[128], sq_chord_pitch[SQ_NOTES], sq_chord_vel[SQ_NOTES];
static uint32_t sq_chord_n, sq_chord_down, sq_chord_target, sq_changed = 1, sq_idle_ok;

static uint32_t sq_shift(void)
{
    return sq_shift_down || sq_shift_latch;
}
static int32_t sq_target(void)
{
    return sq_record       ? (int32_t)sq_cursor
           : sq_temp >= 0  ? sq_temp
           : sq_edit >= 0  ? sq_edit
           : sq_latch >= 0 ? sq_latch
                           : sq_note_latch_step;
}
static int32_t sq_clamp(int32_t n, int32_t lo, int32_t hi)
{
    return n < lo ? lo : n > hi ? hi : n;
}
static uint32_t sq_count(const sq_step_t *s)
{
    uint32_t n = 0, i;
    for (i = 0; i < SQ_NOTES; i++)
        n += !!s->note[i].used;
    return n;
}
static void sq_on(uint32_t n, uint32_t v)
{
    if (n < 128u)
    {
        if (sq_refs[n] < 255u)
            sq_refs[n]++;
        play(n, v);
    }
}
static void sq_off(uint32_t n)
{
    if (n < 128u && sq_refs[n] && !--sq_refs[n])
        stop(n);
}
static void sq_seq_off(void)
{
    while (sq_seq_count)
        sq_off(sq_seq_notes[--sq_seq_count]);
}
static void sq_panic(void)
{
    uint32_t i;
    for (i = 0; i < 128u; i++)
    {
        sq_refs[i] = 0;
        sq_live[i] = 0;
        sq_capture[i] = 0;
        stop(i);
    }
    sq_seq_count = sq_chord_down = sq_chord_n = 0;
}
static void sq_select(uint32_t step)
{
    sq_step = step;
    sq_slot = 0;
    while (sq_slot + 1u < SQ_NOTES && !sq_steps[step].note[sq_slot].used)
        sq_slot++;
    if (!sq_steps[step].note[sq_slot].used)
        sq_slot = 0;
    sq_changed = 1;
}
static sq_note_t sq_default(void)
{
    sq_note_t n = {1, 60, 100, 100, 80};
    return n;
}
static void sq_init(void)
{
    uint32_t i;
    for (i = 0; i < SQ_STEPS; i++)
    {
        sq_steps[i].length = 1;
        sq_steps[i].note[0] = sq_default();
    }
    for (i = 0; i < 27u; i++)
        sq_origin[i] = -2;
}
static void sq_toggle(uint32_t i)
{
    sq_step_t *s = &sq_steps[i];
    if (!sq_count(s))
    {
        if (!s->note[0].velocity)
            s->note[0] = sq_default();
        else
            s->note[0].used = 1;
    }
    s->enabled = !s->enabled;
    sq_changed = 1;
}
/* A hold consumes the immediate tap preview, preserving the original step state. */
static void sq_step_hold(uint32_t k)
{
    if (sq_origin[k] >= 0 && sq_origin[k] < 128 && !sq_used[k])
    {
        uint32_t i;
        sq_steps[sq_origin[k]].enabled = (uint8_t)sq_before[k];
        for (i = 0; i < SQ_NOTES; i++)
            sq_steps[sq_origin[k]].note[i].used = (uint8_t)(sq_step_notes_before[k] >> i & 1u);
    }
    sq_used[k] = 1;
    sq_changed = 1;
}
static void sq_use_step(void)
{
    uint32_t k;
    for (k = 0; k < 27u; k++)
        if (sq_origin[k] >= 0 && sq_origin[k] < 128)
            sq_step_hold(k);
}
static void sq_note_toggle(uint32_t target, uint32_t slot)
{
    sq_step_t *s = &sq_steps[target];
    sq_note_t *n = &s->note[slot];
    if (!n->velocity)
    {
        *n = s->note[sq_slot].velocity ? s->note[sq_slot] : sq_default();
        n->used = 0;
    }
    n->used = !n->used;
    if (n->used)
        s->enabled = 1;
    else if (!sq_count(s))
        s->enabled = 0;
    sq_step = target;
    sq_slot = slot;
    sq_changed = 1;
}
static void sq_note_hold(uint32_t k, uint32_t slot)
{
    if (!sq_used[k])
    {
        sq_step_t *s = &sq_steps[sq_note_step[k]];
        s->note[slot].used = (uint8_t)sq_before[k];
        s->enabled = (uint8_t)sq_parent_before[k];
    }
    sq_slot = slot;
    sq_step = sq_note_step[k];
    sq_used[k] = 1;
    sq_changed = 1;
}
static uint32_t sq_duration(void)
{
    uint32_t ms = 15000u / sq_bpm;
    sq_rem += 15000u % sq_bpm;
    if (sq_rem >= sq_bpm)
    {
        sq_rem -= sq_bpm;
        ms++;
    }
    return ms;
}
static uint32_t sq_seq_deadline[NV * 16u];
static void sq_start_step(void)
{
    uint32_t i, n = 0;
    sq_step_t *s = &sq_steps[sq_head];
    if (s->enabled)
        for (i = 0; i < SQ_NOTES && n < NV; i++)
            if (s->note[i].used)
            {
                sq_note_t *p = &s->note[i];
                sq_rand = sq_rand * 1664525u + 1013904223u;
                if ((sq_rand >> 16) % 100u < p->probability && sq_seq_count < NV * 16u)
                {
                    sq_seq_notes[sq_seq_count] = p->pitch;
                    sq_seq_deadline[sq_seq_count++] = fm1_ms + (15000u / sq_bpm) * s->length * p->gate / 100u;
                    sq_on(p->pitch, p->velocity);
                    n++;
                }
            }
    sq_changed = 1;
}
static void sq_transport(uint32_t on)
{
    sq_seq_off();
    sq_playing = on;
    if (on)
    {
        sq_head = 0;
        sq_rem = 0;
        sq_next = fm1_ms + sq_duration();
        sq_start_step();
    }
    sq_changed = 1;
}
static void sq_service(void)
{
    uint32_t j;
    for (j = 0; j < sq_seq_count;)
        if ((int32_t)(fm1_ms - sq_seq_deadline[j]) >= 0)
        {
            sq_off(sq_seq_notes[j]);
            sq_seq_count--;
            sq_seq_notes[j] = sq_seq_notes[sq_seq_count];
            sq_seq_deadline[j] = sq_seq_deadline[sq_seq_count];
        }
        else
            j++;
    if (sq_playing && (int32_t)(fm1_ms - sq_next) >= 0)
    {
        sq_head = (sq_head + 1u) % (sq_pages * 16u);
        sq_start_step();
        sq_next += sq_duration();
        if ((int32_t)(fm1_ms - sq_next) >= 0)
            sq_next = fm1_ms + sq_duration();
    }
}
static void sq_commit(void)
{
    uint32_t i;
    sq_step_t *s;
    if (!sq_chord_n)
        return;
    s = &sq_steps[sq_chord_target];
    for (i = 0; i < SQ_NOTES; i++)
        s->note[i].used = 0;
    for (i = 0; i < sq_chord_n; i++)
    {
        s->note[i] = sq_default();
        s->note[i].pitch = sq_chord_pitch[i];
        s->note[i].velocity = sq_chord_vel[i];
    }
    s->enabled = 1;
    sq_select(sq_chord_target);
    sq_chord_n = 0;
    if (sq_record)
    {
        sq_cursor += s->length;
        if (sq_cursor >= SQ_STEPS)
            sq_cursor = 0;
        if (sq_cursor / 16u >= sq_pages)
            sq_pages = sq_cursor / 16u + 1u;
        sq_page = sq_cursor / 16u;
        sq_select(sq_cursor);
    }
    sq_changed = 1;
}
static void sq_musical_on(uint32_t note, uint32_t vel)
{
    int32_t target = sq_target();
    if (target >= 0)
        sq_use_step();
    uint32_t i;
    if (note > 127u)
        return;
    sq_on(note, vel);
    if (sq_live[note] < 255u)
        sq_live[note]++;
    if (target < 0)
        return;
    if (!sq_chord_down)
    {
        sq_chord_n = 0;
        sq_chord_target = (uint32_t)target;
    }
    sq_chord_down++;
    sq_capture[note]++;
    for (i = 0; i < sq_chord_n; i++)
        if (sq_chord_pitch[i] == note)
            return;
    if (sq_chord_n < SQ_NOTES)
    {
        sq_chord_pitch[sq_chord_n] = (uint8_t)note;
        sq_chord_vel[sq_chord_n++] = (uint8_t)vel;
    }
}
static void sq_musical_off(uint32_t note)
{
    if (note > 127u || !sq_live[note])
        return;
    sq_live[note]--;
    sq_off(note);
    if (sq_capture[note])
    {
        sq_capture[note]--;
        if (sq_chord_down && !--sq_chord_down)
            sq_commit();
    }
}
static void sq_record_enter(int32_t anchor)
{
    if (anchor < 0)
        return;
    sq_record = 1;
    sq_cursor = (uint32_t)anchor;
    sq_latch = -1;
    sq_note_latch = sq_note_latch_step = -1;
    sq_temp = -1;
    sq_select(sq_cursor);
    sq_changed = 1;
}
static void sq_shift_press(void)
{
    int32_t target = sq_target();
    sq_shift_down = 1;
    if (!sq_shift_tap)
    {
        uint32_t i;
        sq_anchor = target;
        if (target >= 0)
        {
            sq_temp = target;
            for (i = 0; i < 27u; i++)
                if (sq_origin[i] == target)
                    sq_step_hold(i);
        }
    }
    if (sq_shift_tap && fm1_ms - sq_shift_at <= SQ_TAP)
    {
        sq_shift_tap = 0;
        sq_shift_latch = !sq_shift_latch;
        if (sq_shift_latch)
            sq_record_enter(sq_anchor);
        else
        {
            sq_record = 0;
            sq_temp = -1;
        }
    }
    else
    {
        sq_shift_tap = 1;
        sq_shift_at = fm1_ms;
    }
    sq_changed = 1;
}
static void sq_shift_release(void)
{
    sq_shift_down = 0;
    if (!sq_shift_latch)
        sq_temp = -1;
    sq_changed = 1;
}
static void sq_midi(void)
{
    uint8_t st, a, b;
    while (midi_get(&st, &a, &b))
    {
        if ((st & 0xF0u) == 0x90u && b)
            sq_musical_on(a, b);
        else if ((st & 0xF0u) == 0x80u || (st & 0xF0u) == 0x90u)
            sq_musical_off(a);
        else if ((st & 0xF0u) == 0xB0u && (a == 120u || a == 123u))
        {
            sq_transport(0);
            sq_panic();
        }
    }
}
static void sq_poll_input(void);
static void app_idle(void)
{
    if (sq_idle_ok)
    {
        sq_poll_input();
        sq_service();
    }
}
static void sq_keys(uint32_t keys)
{
    uint32_t down = keys & ~sq_prev, up = sq_prev & ~keys, k, w;
    sq_prev = keys;
    for (k = 0; k < 27u; k++)
    {
        if (down >> k & 1u)
        {
            sq_down_at[k] = fm1_ms;
            sq_used[k] = 0;
            if (sq_shift() || sq_record)
            {
                uint32_t pitch = (uint32_t)sq_clamp(53 + (int32_t)k + sq_oct * 12, 0, 127);
                sq_origin[k] = 128 + (int32_t)pitch;
                sq_musical_on(pitch, 100);
            }
            else
            {
                for (w = 0; w < 16u && sq_white[w] != k; w++)
                {
                }
                if (w < 16u)
                {
                    uint32_t at = sq_page * 16u + w;
                    sq_origin[k] = (int32_t)at;
                    sq_edit = (int32_t)at;
                    sq_note_latch = sq_note_latch_step = -1;
                    sq_bank = 0;
                    sq_select(at);
                    sq_before[k] = sq_steps[at].enabled;
                    sq_step_notes_before[k] = 0;
                    {
                        uint32_t i;
                        for (i = 0; i < SQ_NOTES; i++)
                            sq_step_notes_before[k] |= (uint32_t)!!sq_steps[at].note[i].used << i;
                    }
                    if (sq_tap_pending[w] && sq_tap_step[w] == at && fm1_ms - sq_tap_at[w] <= SQ_TAP)
                    {
                        sq_steps[at].enabled = (uint8_t)sq_tap_before[w];
                        sq_before[k] = sq_tap_before[w];
                        sq_step_notes_before[k] = sq_tap_notes_before[w];
                        {
                            uint32_t i;
                            for (i = 0; i < SQ_NOTES; i++)
                                sq_steps[at].note[i].used = (uint8_t)(sq_tap_notes_before[w] >> i & 1u);
                        }
                        sq_tap_pending[w] = 0;
                        sq_latch = sq_latch == (int32_t)at ? -1 : (int32_t)at;
                        sq_used[k] = 1;
                    }
                    else
                    {
                        sq_tap_pending[w] = 0;
                        sq_toggle(at); /* Immediate response; hold/double restores this preview. */
                    }
                }
                else
                {
                    int32_t target = sq_target();
                    for (w = 0; w < SQ_NOTES && sq_black[w] != k; w++)
                    {
                    }
                    sq_origin[k] = -2;
                    if (w < SQ_NOTES && target >= 0)
                    {
                        sq_use_step();
                        sq_origin[k] = -1;
                        sq_note_step[k] = (uint32_t)target;
                        sq_before[k] = sq_steps[target].note[w].used;
                        sq_parent_before[k] = sq_steps[target].enabled;
                        if (sq_note_tap[w] && sq_note_tap_step[w] == (uint32_t)target &&
                            fm1_ms - sq_note_tap_at[w] <= SQ_TAP)
                        {
                            sq_steps[target].note[w].used = (uint8_t)sq_note_tap_before[w];
                            sq_steps[target].enabled = (uint8_t)sq_note_tap_parent[w];
                            if (sq_note_latch == (int32_t)w && sq_note_latch_step == target)
                                sq_note_latch = sq_note_latch_step = -1;
                            else
                            {
                                sq_note_latch = (int32_t)w;
                                sq_note_latch_step = target;
                            }
                            sq_used[k] = 1;
                            sq_note_tap[w] = 0;
                            sq_slot = w;
                        }
                        else
                        {
                            sq_note_tap[w] = 0;
                            sq_note_toggle((uint32_t)target, w);
                        }
                        sq_changed = 1;
                    }
                }
            }
        }
        if ((keys >> k & 1u) && !sq_used[k] && fm1_ms - sq_down_at[k] >= SQ_HOLD)
        {
            if (sq_origin[k] >= 0 && sq_origin[k] < 128)
                sq_step_hold(k);
            else if (sq_origin[k] == -1)
            {
                for (w = 0; w < SQ_NOTES && sq_black[w] != k; w++)
                {
                }
                if (w < SQ_NOTES)
                    sq_note_hold(k, w);
            }
        }
        if (up >> k & 1u)
        {
            int32_t origin = sq_origin[k];
            if (origin >= 128)
                sq_musical_off((uint32_t)(origin - 128));
            else if (origin == -1 && !sq_used[k])
            {
                for (w = 0; w < SQ_NOTES && sq_black[w] != k; w++)
                {
                }
                if (w < SQ_NOTES)
                {
                    sq_note_tap[w] = 1;
                    sq_note_tap_at[w] = fm1_ms;
                    sq_note_tap_step[w] = sq_note_step[k];
                    sq_note_tap_before[w] = sq_before[k];
                    sq_note_tap_parent[w] = sq_parent_before[k];
                }
            }
            else if (origin >= 0)
            {
                w = (uint32_t)origin % 16u;
                if (!sq_used[k])
                {
                    sq_tap_pending[w] = 1;
                    sq_tap_at[w] = fm1_ms;
                    sq_tap_step[w] = (uint32_t)origin;
                    sq_tap_before[w] = sq_before[k];
                    sq_tap_notes_before[w] = sq_step_notes_before[k];
                }
                if (sq_edit == origin)
                    sq_edit = -1;
            }
            sq_origin[k] = -2;
            sq_changed = 1;
        }
    }
    if (sq_shift_tap && fm1_ms - sq_shift_at > SQ_TAP)
        sq_shift_tap = 0;
}
enum
{
    SP_NONE,
    SP_TEMPO,
    SP_PITCH,
    SP_VEL,
    SP_CHANCE,
    SP_LENGTH,
    SP_GATE,
    SP_SLOT,
    SP_PAGE,
    SP_OCT,
    SP_LOOP
};
static uint32_t sq_family(void)
{
    return sq_menu + (sq_shift() ? 3u : 0u);
}
static uint32_t sq_param(uint32_t k)
{
    static const uint8_t banks[8][4] = {
        {SP_LENGTH, SP_VEL, SP_CHANCE, SP_PITCH}, {SP_PITCH, SP_VEL, SP_GATE, SP_CHANCE},
        {SP_NONE, SP_NONE, SP_LENGTH, SP_VEL},    {SP_GATE, SP_CHANCE, SP_VEL, SP_LENGTH},
        {SP_LENGTH, SP_VEL, SP_CHANCE, SP_GATE},  {SP_PITCH, SP_GATE, SP_VEL, SP_CHANCE},
        {SP_NONE, SP_NONE, SP_GATE, SP_CHANCE},   {SP_OCT, SP_LENGTH, SP_PITCH, SP_GATE}};
    if (sq_target() >= 0)
    {
        if (k == ENC_PRESETS)
            return sq_shift() ? SP_OCT : SP_SLOT;
        if (k == ENC_SELECT)
            return sq_shift() ? SP_GATE : SP_LENGTH;
        if (k == ENC_ALGORITHM)
            return sq_shift() ? SP_SLOT : SP_GATE;
        return banks[sq_bank + (sq_shift() ? 4u : 0u)][k - ENC_KNOB1];
    }
    if (k == ENC_PRESETS)
        return sq_shift() ? SP_OCT : sq_menu == 1 ? SP_PAGE : sq_menu == 2 ? SP_SLOT : SP_PAGE;
    if (k == ENC_SELECT)
        return sq_menu == 2 ? SP_PITCH : SP_TEMPO;
    if (k == ENC_ALGORITHM)
        return SP_GATE;
    if (sq_shift())
    {
        static const uint8_t alt[3][4] = {{SP_OCT, SP_GATE, SP_VEL, SP_CHANCE},
                                          {SP_OCT, SP_PITCH, SP_VEL, SP_GATE},
                                          {SP_CHANCE, SP_GATE, SP_VEL, SP_LENGTH}};
        return alt[sq_menu][k - ENC_KNOB1];
    }
    if (sq_menu == 2)
    {
        static const uint8_t note[4] = {SP_PITCH, SP_VEL, SP_CHANCE, SP_GATE};
        if (sq_menu_page[2])
        {
            static const uint8_t chord[4] = {SP_NONE, SP_NONE, SP_LENGTH, SP_VEL};
            return chord[k - ENC_KNOB1];
        }
        return note[k - ENC_KNOB1];
    }
    if (sq_menu == 1)
    {
        static const uint8_t pages[4] = {SP_PAGE, SP_TEMPO, SP_LOOP, SP_OCT};
        if (sq_menu_page[1])
        {
            static const uint8_t loop[4] = {SP_LOOP, SP_TEMPO, SP_PAGE, SP_OCT};
            return loop[k - ENC_KNOB1];
        }
        return pages[k - ENC_KNOB1];
    }
    {
        static const uint8_t pattern[4] = {SP_TEMPO, SP_GATE, SP_LOOP, SP_VEL};
        if (sq_menu_page[0])
        {
            static const uint8_t timing[4] = {SP_TEMPO, SP_NONE, SP_GATE, SP_CHANCE};
            return timing[k - ENC_KNOB1];
        }
        return pattern[k - ENC_KNOB1];
    }
}
static void sq_encoders(void)
{
    int32_t e;
    uint32_t k;
    for (k = 0; k < 7u; k++)
        if ((e = fm1_enc_take(k)) != 0)
        {
            uint32_t j, p;
            int32_t target;
            sq_step_t *s;
            sq_note_t *n;
            sq_use_step();
            for (j = 0; j < 27u; j++)
                if (sq_origin[j] == -1)
                {
                    uint32_t w;
                    for (w = 0; w < SQ_NOTES && sq_black[w] != j; w++)
                    {
                    }
                    if (w < SQ_NOTES)
                        sq_note_hold(j, w);
                }
            target = sq_target();
            s = &sq_steps[target >= 0 ? (uint32_t)target : sq_step];
            n = &s->note[sq_slot];
            p = sq_param(k);
            switch (p)
            {
            case SP_TEMPO:
                sq_bpm = (uint32_t)sq_clamp((int32_t)sq_bpm + e, 30, 300);
                sq_rem = 0;
                break;
            case SP_SLOT:
                sq_slot = (uint32_t)sq_clamp((int32_t)sq_slot + e, 0, 10);
                break;
            case SP_PITCH:
                if (n->velocity)
                    n->pitch = (uint8_t)sq_clamp((int32_t)n->pitch + e, 0, 127);
                break;
            case SP_GATE:
                if (n->velocity)
                    n->gate = (uint8_t)sq_clamp((int32_t)n->gate + e, 1, 100);
                break;
            case SP_LENGTH:
                s->length = (uint8_t)sq_clamp((int32_t)s->length + e, 1, 16);
                break;
            case SP_VEL:
                if (n->velocity)
                    n->velocity = (uint8_t)sq_clamp((int32_t)n->velocity + e, 1, 127);
                break;
            case SP_CHANCE:
                if (n->velocity)
                    n->probability = (uint8_t)sq_clamp((int32_t)n->probability + e, 0, 100);
                break;
            case SP_OCT:
                sq_oct = sq_clamp(sq_oct + e, -2, 2);
                break;
            case SP_PAGE:
                sq_page = (uint32_t)sq_clamp((int32_t)sq_page + e, 0, (int32_t)sq_pages - 1);
                break;
            case SP_LOOP:
                sq_pages = (uint32_t)sq_clamp((int32_t)sq_pages + e, 1, SQ_PAGES);
                if (sq_page >= sq_pages)
                    sq_page = sq_pages - 1;
                break;
            default:
                break;
            }
            sq_changed = 1;
        }
}
static char *sq_uint(char *p, uint32_t n)
{
    if (n >= 1000u)
        *p++ = (char)('0' + n / 1000u % 10u);
    if (n >= 100u)
        *p++ = (char)('0' + n / 100u % 10u);
    if (n >= 10u)
        *p++ = (char)('0' + n / 10u % 10u);
    *p++ = (char)('0' + n % 10u);
    *p = 0;
    return p;
}
/* Cache stable fields: only changed text reaches LCD DMA. */
static struct
{
    char text[40];
    uint16_t color;
    uint8_t valid;
} sq_text_cache[32];
static uint32_t sq_text_index;
static void sq_text(uint32_t x, uint32_t y, uint32_t width, const char *text, uint16_t color, uint32_t align)
{
    uint32_t i = 0, changed = 0, index = sq_text_index++;
    if (index >= 32u)
        return;
    do
    {
        if (sq_text_cache[index].text[i] != text[i])
            changed = 1;
    } while (text[i++] && i < 39u);
    if (!changed && sq_text_cache[index].valid && sq_text_cache[index].color == color)
        return;
    i = 0;
    do
    {
        sq_text_cache[index].text[i] = text[i];
    } while (text[i++] && i < 39u);
    sq_text_cache[index].text[39] = 0;
    sq_text_cache[index].valid = 1;
    sq_text_cache[index].color = color;
    draw_text_box(x, y, width, &FONT_S, text, color, align);
}
static const char *sq_soft_label(uint32_t i)
{
    static const char *const normal[4] = {"PAGES", "DUP PG", "CHORD", "NOTE"};
    static const char *const edit[8] = {"STEP", "NOTE", "CHORD", "MORE", "GATE", "EXPR", "STRUM", "RANGE"};
    static const char *const shifted[4] = {"OCT 0", "N/A", "MIDI", "PANIC"};
    if (sq_target() < 0 && !sq_shift() && i == 1 && sq_pages == SQ_PAGES)
        return "FULL";
    return sq_target() >= 0 ? edit[i + (sq_shift() ? 4u : 0u)] : sq_shift() ? shifted[i] : normal[i];
}
static void sq_soft(uint32_t i)
{
    if (sq_target() >= 0)
    {
        sq_use_step();
        sq_bank = i;
    }
    else if (sq_shift())
    {
        if (i == 0)
            sq_oct = 0;
        if (i == 2)
        {
            sq_menu = 1;
            sq_menu_page[4] = 1;
        }
        if (i == 3)
        {
            sq_transport(0);
            sq_panic();
        }
    }
    else
    {
        if (i == 0)
        {
            sq_menu = 1;
            sq_menu_page[1] = 0;
        }
        if (i == 1 && sq_pages < SQ_PAGES)
        {
            uint32_t j;
            for (j = 0; j < 16u; j++)
                sq_steps[sq_pages * 16u + j] = sq_steps[sq_page * 16u + j];
            sq_page = sq_pages++;
        }
        if (i == 2)
        {
            sq_menu = 2;
            sq_menu_page[2] = 1;
        }
        if (i == 3)
        {
            sq_menu = 2;
            sq_menu_page[2] = 0;
        }
    }
    sq_changed = 1;
}
static void sq_knob_value(uint32_t x, uint32_t y, uint32_t width, uint32_t k)
{
    static const char *const labels[] = {"N/A",  "BPM",  "PITCH", "VEL", "PROB", "LEN",
                                         "GATE", "SLOT", "PAGE",  "OCT", "LOOP"};
    uint32_t p = sq_param(k), value = 0, valid = 1;
    char text[12] = "--";
    int32_t target = sq_target();
    sq_step_t *step = &sq_steps[target >= 0 ? (uint32_t)target : sq_step];
    sq_note_t *n = &step->note[sq_slot];
    switch (p)
    {
    case SP_TEMPO:
        value = sq_bpm;
        break;
    case SP_PITCH:
        value = n->pitch;
        valid = !!n->velocity;
        break;
    case SP_VEL:
        value = n->velocity;
        valid = !!n->velocity;
        break;
    case SP_CHANCE:
        value = n->probability;
        valid = !!n->velocity;
        break;
    case SP_LENGTH:
        value = step->length;
        break;
    case SP_GATE:
        value = n->gate;
        valid = !!n->velocity;
        break;
    case SP_SLOT:
        value = sq_slot + 1u;
        break;
    case SP_PAGE:
        value = sq_page + 1u;
        break;
    case SP_LOOP:
        value = sq_pages;
        break;
    case SP_OCT:
        value = (uint32_t)(sq_oct < 0 ? -sq_oct : sq_oct);
        break;
    default:
        valid = 0;
        break;
    }
    if (valid)
    {
        if (p == SP_OCT)
        {
            text[0] = sq_oct < 0 ? '-' : '+';
            sq_uint(text + 1, value);
        }
        else
            sq_uint(text, value);
    }
    sq_text(x, y, width, labels[p], SQ_DIM, 1);
    sq_text(x, y + 16u, width, text, C_WHITE, 1);
}
static void sq_draw(void)
{
    static const char *const title[6][2] = {{"Pattern", "Timing"},      {"Pages", "Loop"},
                                            {"Note", "Chord / block"},  {"Performance", "Utilities"},
                                            {"Keyboard", "MIDI input"}, {"Modulation", "Advanced"}};
    static uint32_t header_drawn = ~0u, steps_drawn = ~0u, slots_drawn = ~0u;
    static const char *const physical[4] = {"HOME", "SAVE", "ARP", "SEQ"};
    static const char *const menu_name[3] = {"FX", "SEL", "ENV"};
    char text[40], *p;
    uint32_t i, family = sq_family(), head = family * 2u + sq_menu_page[family], mask = 0, slotmask = 0;
    int32_t target = sq_target();
    sq_text_index = 0;
    if (head != header_drawn)
    {
        header_drawn = head;
        cv_begin(240, 19, C_BLACK);
        for (i = 0; i < 3u; i++)
        {
            uint32_t active = i == sq_menu;
            char label[12], *q = label;
            const char *name = menu_name[i];
            while (*name)
                *q++ = *name++;
            if (active)
            {
                *q++ = ' ';
                q = sq_uint(q, sq_menu_page[family] + 1u);
                *q++ = '/';
                *q++ = '2';
            }
            *q = 0;
            cv_rect((int32_t)i * 80, 0, 79, 19, active ? SQ_ACC : C_BLACK);
            cv_text((int32_t)i * 80 + 4, 1, &FONT_S, label, active ? C_BLACK : SQ_DIM);
        }
        cv_blit(0, 0);
    }
    sq_text(0, 21, 240, title[family][sq_menu_page[family]], C_WHITE, 1);
    p = text;
    if (target >= 0)
    {
        *p++ = 'S';
        p = sq_uint(p, (uint32_t)target + 1u);
        *p++ = ' ';
        *p++ = 'N';
        p = sq_uint(p, sq_slot + 1u);
        *p++ = ' ';
        const char *a = sq_note_latch >= 0 ? "NOTE LOCK"
                        : sq_record        ? "RECORD"
                        : sq_shift()       ? "KEYS"
                                           : "EDIT";
        while (*a)
            *p++ = *a++;
    }
    else
    {
        const char *a = sq_shift() ? "SHIFT / KEYBOARD" : "PATTERN";
        while (*a)
            *p++ = *a++;
    }
    *p = 0;
    sq_text(0, 41, 240, text, SQ_ACC, 1);
    sq_knob_value(0, 63, 80, ENC_SELECT);
    sq_knob_value(80, 63, 80, ENC_ALGORITHM);
    sq_knob_value(160, 63, 80, ENC_PRESETS);
    for (i = 0; i < 4u; i++)
        sq_knob_value(i * 60u, 103, 60, ENC_KNOB1 + i);
    if (target >= 0 && !sq_shift())
    {
        for (i = 0; i < SQ_NOTES; i++)
            if (sq_steps[target].note[i].used)
                slotmask |= 1u << i;
        slotmask |= (sq_slot + 1u) << 12;
    }
    if (slotmask != slots_drawn)
    {
        slots_drawn = slotmask;
        cv_begin(240, 19, C_BLACK);
        if (target >= 0 && !sq_shift())
            for (i = 0; i < SQ_NOTES; i++)
            {
                char label[4];
                sq_uint(label, i + 1u);
                cv_rect((int32_t)i * 21 + 4, 0, 19, 18, slotmask >> i & 1u ? SQ_ACC : RGB(40, 40, 40));
                cv_text((int32_t)i * 21 + 5, 1, &FONT_S, label, slotmask >> i & 1u ? C_BLACK : SQ_DIM);
                if (i == sq_slot)
                    cv_line((int32_t)i * 21 + 4, 18, (int32_t)i * 21 + 22, 18, C_WHITE);
            }
        cv_blit(0, 143);
    }
    for (i = 0; i < 16u; i++)
        if (sq_steps[sq_page * 16u + i].enabled)
            mask |= 1u << i;
    mask |= (sq_playing && sq_head / 16u == sq_page ? (sq_head % 16u + 1u) : 0u) << 16;
    if (mask != steps_drawn)
    {
        steps_drawn = mask;
        cv_begin(240, 17, C_BLACK);
        for (i = 0; i < 16u; i++)
            cv_rect((int32_t)i * 15, 0, 13, 16,
                    sq_playing && sq_head == sq_page * 16u + i ? SQ_ACC
                    : mask >> i & 1u                           ? C_WHITE
                                                               : RGB(55, 55, 55));
        cv_blit(0, 168);
    }
    p = text;
    p = sq_uint(p, sq_bpm);
    *p++ = ' ';
    *p++ = 'P';
    p = sq_uint(p, sq_page + 1u);
    *p++ = '/';
    p = sq_uint(p, sq_pages);
    *p++ = ' ';
    p = sq_uint(p, sq_pages * 16u);
    *p++ = '/';
    p = sq_uint(p, SQ_STEPS);
    *p++ = ' ';
    *p++ = 'R';
    *p++ = 'A';
    *p++ = 'M';
    *p = 0;
    sq_text(0, 188, 240, text, SQ_DIM, 1);
    for (i = 0; i < 4u; i++)
    {
        sq_text(i * 60u, 207, 60, physical[i], SQ_DIM, 1);
        sq_text(i * 60u, 224, 60, sq_soft_label(i), target >= 0 && sq_bank == i ? SQ_ACC : C_WHITE, 1);
    }
}
static void sq_buttons(uint32_t b, uint32_t rel)
{
    int32_t target;
    if (b & (1u << BTN_GLO))
        sq_shift_press();
    if (rel & (1u << BTN_GLO))
        sq_shift_release();
    {
        uint32_t i;
        static const uint8_t main_buttons[3] = {BTN_FX, BTN_SEL, BTN_ENV};
        static const uint8_t soft_buttons[4] = {BTN_HOME, BTN_SAVE, BTN_ARP, BTN_SEQ};
        for (i = 0; i < 3u; i++)
            if (b & (1u << main_buttons[i]))
            {
                sq_use_step();
                if (sq_menu == i)
                    sq_menu_page[sq_family()] ^= 1u;
                else
                    sq_menu = i;
                sq_changed = 1;
            }
        for (i = 0; i < 4u; i++)
            if (b & (1u << soft_buttons[i]))
                sq_soft(i);
    }
    if (b & (1u << BTN_PLAY))
        sq_transport(!sq_playing);
    if (b & (1u << BTN_REC))
    {
        if (sq_record)
        {
            sq_record = 0;
            sq_shift_latch = 0;
            sq_temp = -1;
        }
        else
        {
            target = sq_target();
            if (target >= 0)
            {
                sq_shift_latch = 1;
                sq_record_enter(target);
            }
        }
        sq_changed = 1;
    }
    if (b & (1u << BTN_LFO))
    {
        sq_record = sq_shift_latch = 0;
        sq_temp = sq_latch = -1;
        sq_note_latch = sq_note_latch_step = -1;
        sq_panic();
        sq_changed = 1;
    }
    if (b & ((1u << BTN_OCT_DOWN) | (1u << BTN_OCT_UP)))
    {
        int32_t d = b & (1u << BTN_OCT_UP) ? 1 : -1;
        if (sq_shift() || sq_record)
            sq_oct = sq_clamp(sq_oct + d, -2, 2);
        else
        {
            sq_page = (uint32_t)sq_clamp((int32_t)sq_page + d, 0, SQ_PAGES - 1u);
            if (sq_page >= sq_pages)
                sq_pages = sq_page + 1u;
        }
        sq_changed = 1;
    }
}
static void sq_leds(void)
{
    uint32_t k, w;
    int32_t target = sq_target();
    for (k = 0; k < 27u; k++)
    {
        uint32_t on = 0;
        for (w = 0; w < 16u; w++)
            if (sq_white[w] == k)
                on = sq_shift() ? !!(fm1_in.notes >> k & 1u)
                                : sq_steps[sq_page * 16u + w].enabled ||
                                      (sq_playing && sq_head == sq_page * 16u + w);
        for (w = 0; w < SQ_NOTES; w++)
            if (sq_black[w] == k)
                on = sq_shift() ? !!(fm1_in.notes >> k & 1u) : target >= 0 && sq_steps[target].note[w].used;
        fm1_led_key(KEY_LED(k), on);
    }
    fm1_led_key(BTN_PLAY, sq_playing);
    fm1_led_key(BTN_GLO, sq_shift());
    fm1_led_key(BTN_REC, sq_record);
    fm1_led_key(BTN_FX, sq_menu == 0);
    fm1_led_key(BTN_SEL, sq_menu == 1);
    fm1_led_key(BTN_ENV, sq_menu == 2);
}
static void sq_poll_input(void)
{
    uint32_t rel, b = fm1_input_edges(&rel);
    sq_buttons(b, rel);
    sq_keys(fm1_in.notes | fm1_input_note_edges());
    sq_encoders();
    sq_midi();
    sq_leds();
}
static void app_main(void)
{
    uint32_t last_draw = 0;
    sq_init();
    lcd_fill(0, 0, 240, 240, C_BLACK);
    for (;;)
    {
        platform_poll();
        sq_poll_input();
        sq_service();
        sq_leds();
        if (sq_changed && fm1_ms - last_draw >= 16u)
        {
            sq_changed = 0;
            last_draw = fm1_ms;
            sq_idle_ok = 1;
            sq_draw();
            sq_idle_ok = 0;
        }
    }
}
