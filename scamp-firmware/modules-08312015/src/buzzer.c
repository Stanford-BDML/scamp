/**
 *    ||          ____  _ __
 * +------+      / __ )(_) /_______________ _____  ___
 * | 0xBC |     / __  / / __/ ___/ ___/ __ `/_  / / _ \
 * +------+    / /_/ / / /_/ /__/ /  / /_/ / / /_/  __/
 *  ||  ||    /_____/_/\__/\___/_/   \__,_/ /___/\___/
 *
 * Crazyflie control firmware
 *
 * Copyright (C) 2012 BitCraze AB
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, in version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * neopixelring.c: NeoPixel Ring 12 Leds effects/driver
 */

#include "neopixelring.h"

#include <stdint.h>
#include <stdlib.h>
#include "stm32fxxx.h"

#include "FreeRTOS.h"
#include "timers.h"

#include "param.h"
#include "pm.h"
#include "log.h"
#include "piezo.h"
#include "buzzer.h"

typedef void (*BuzzerEffect)(uint32_t timer);

/**
 * Credit to http://tny.cz/e525c1b2 for supplying the tones
 */
#define C0 16
#define Db0 17
#define D0  18
#define Eb0 19
#define E0  20
#define F0  21
#define Gb0 23
#define G0  24
#define Ab0 25
#define A0 27
#define Bb0 29
#define B0  30
#define C1  32
#define Db1 34
#define D1  36
#define Eb1 38
#define E1  41
#define F1  43
#define Gb1 46
#define G1  49
#define Ab1 51
#define A1 55
#define Bb1 58
#define B1  61
#define C2  65
#define Db2 69
#define D2  73
#define Eb2 77
#define E2  82
#define F2  87
#define Gb2 92
#define G2  98
#define Ab2 103
#define A2 110
#define Bb2 116
#define B2  123
#define C3  130
#define Db3 138
#define D3  146
#define Eb3 155
#define E3  164
#define F3  174
#define Gb3 185
#define G3  196
#define Ab3 207
#define A3 220
#define Bb3 233
#define B3  246
#define C4  261
#define Db4 277
#define D4  293
#define Eb4 311
#define E4  329
#define F4  349
#define Gb4 369
#define G4  392
#define Ab4 415
#define A4 440
#define Bb4 466
#define B4  493
#define C5  523
#define Db5 554
#define D5  587
#define Eb5 622
#define E5  659
#define F5  698
#define Gb5 739
#define G5  783
#define Ab5 830
#define A5 880
#define Bb5 932
#define B5  987
#define C6  1046
#define Db6 1108
#define D6  1174
#define Eb6 1244
#define E6  1318
#define F6  1396
#define Gb6 1479
#define G6  1567
#define Ab6 1661
#define A6 1760
#define Bb6 1864
#define B6  1975
#define C7  2093
#define Db7 2217
#define D7  2349
#define Eb7 2489
#define E7  2637
#define F7  2793
#define Gb7 2959
#define G7  3135
#define Ab7 3322
#define A7 3520
#define Bb7 3729
#define B7  3951
#define C8  4186
#define Db8 4434
#define D8  4698
#define Eb8 4978
/* Duration of notes */
#define W (60)
#define H (W * 2)
#define Q (W * 4)
#define E (W * 8)
#define S (W * 16)
#define ES (W*6)

#define FULL_OUT 150
#define FULL_UP 95
#define MID_OUT 130
#define MID_UP 130
#define DEFAULT_MS_PER_STEP 30
//#define HORIZONTAL_IN (1)
//#define SLANT_IN (2)
//#define VERTICAL_DOWN (3)
//#define HORIZONTAL_OUT (4)
//#define SLANT_OUT (5)
//#define VERTICAL_UP (6)


typedef struct {
  uint16_t tone;
  uint16_t duration;
} Note;

typedef struct {
  uint32_t bpm;
  uint32_t ni;
  uint32_t delay;
  Note notes[80];
} Melody;

Melody melodies[] = {
    {.bpm = 120, .ni = 0, .delay = 1, .notes = {{C4, H}, {D4, H}, {E4, H}, {F4, H}, {G4, H}, {A4, H}, {B4, H}, {0xFF, 0}}},
    {.bpm = 120, .ni = 0, .delay = 1, .notes = {{C4, S}, {D4, S}, {E4, S}, {F4, S}, {G4, S}, {A4, S}, {B4, S}, {0xFF, 0}}},
    /* Imperial march from http://tny.cz/e525c1b2A */
    {.bpm = 120, .ni = 0, .delay = 1, .notes = {{A3, Q}, {A3, Q}, {A3, Q},{F3, ES}, {C4, S},
                                                {A3, Q}, {F3, ES}, {C4, S}, {A3, H},
                                                {E4, Q}, {E4, Q}, {E4, Q}, {F4, ES}, {C4, S},
                                                {Ab3, Q}, {F3, ES}, {C4, S}, {A3, H},
                                                {A4, Q}, {A3, ES}, {A3, S}, {A4, Q}, {Ab4, ES}, {G4, S},
                                                {Gb4, S}, {E4, S}, {F4, E}, {0, E}, {Bb3, E}, {Eb4, Q}, {D4, ES}, {Db4, S},
                                                {C4, S}, {B3, S}, {C4, E}, {0, E}, {F3, E}, {Ab3, Q}, {F3, ES}, {A3, S},
                                                {C4, Q}, {A3, ES}, {C4, S}, {E4, H},
                                                {A4, Q}, {A3, ES}, {A3, S}, {A4, Q}, {Ab4, ES}, {G4, S},
                                                {Gb4, S}, {E4, S}, {F4, E}, {0, E}, {Bb3, E}, {Eb4, Q}, {D4, ES}, {Db4, S},
                                                {Gb4, S}, {E4, S}, {F4, E}, {0, E}, {Bb3, E}, {Eb4, Q}, {D4, ES}, {Db4, S},
                                                {C4,S}, {B3, S}, {C4, E}, {0, E}, {F3, E}, {Ab3, Q}, {F3, ES}, {C4, S},
                                                {A3, Q}, {F3, ES}, {C4, S}, {A3, H}, {0, H},
                                                {0xFF, 0}}}

};

uint8_t melody = 2;
uint8_t nmelody = 3;
unsigned int mcounter = 0;
static bool playing_sound = false;
static void melodyplayer(uint32_t counter) {
  // Sync one melody for the loop
  Melody* m = &melodies[melody];
  if (mcounter == 0) {
    if (m->notes[m->ni].tone == 0xFF)
    {
      // Loop the melody
      m->ni = 0;
    }
    // Play current note
    piezoSetFreq(m->notes[m->ni].tone);
   // piezoSetRatio(127);
    mcounter = (m->bpm * 100) / m->notes[m->ni].duration;
    playing_sound = true;
    m->ni++;
  }
  else if (mcounter == 1)
  {
     // piezoSetRatio(0);
  }
  mcounter--;
}


static uint8_t gait_state = STAYING_NEUTRAL;

static uint8_t ratio_inout = 0;
static uint8_t ratio_updown = 0;
uint16_t static_freq = 300;

static uint32_t end_move_time = 0;
static uint32_t start_move_time = 0;
static uint32_t steps = 0;
static uint32_t move_ms = 0;
static uint8_t negative_angle = 0;
static uint8_t negative_stroke = 0;
static uint32_t angle_distance;
static uint32_t stroke_distance;

static void move_foot(uint32_t time, uint8_t start_angle, uint8_t start_stroke, uint8_t end_angle, uint8_t end_stroke, uint32_t ms_per_step)
{
	uint32_t current_stroke = 0;
	uint32_t current_angle = 0;
	if (time == end_move_time)
	{
		start_move_time = time;
		if (start_angle < end_angle)
		{
			angle_distance = end_angle - start_angle;
			negative_angle = 0;
		}
		else
		{
			angle_distance = start_angle - end_angle;
			negative_angle = 1;
		}
		if (start_stroke < end_stroke)
		{
			stroke_distance = end_stroke - start_stroke;
			negative_stroke = 0;
		}
		else
		{
			stroke_distance = start_stroke - end_stroke;
			negative_stroke = 1;
		}
		if (angle_distance > stroke_distance)
		{
			steps = angle_distance;
		}
		else
		{
			steps = stroke_distance;
		}
		move_ms = steps*ms_per_step;
		move_ms = move_ms - move_ms%10 + 10;
		end_move_time = start_move_time + move_ms + 10;
	}
	if(negative_angle)
	{
		current_angle = start_angle - (angle_distance*(time-start_move_time))/move_ms;
	}
	else
	{
		current_angle = start_angle + (angle_distance*(time-start_move_time))/move_ms;
	}
	if(negative_stroke)
	{
		current_stroke = start_stroke - (stroke_distance*(time-start_move_time))/move_ms;
	}
	else
	{
		current_stroke = start_stroke + (stroke_distance*(time-start_move_time))/move_ms;
	}
	ratio_inout = current_angle;
	ratio_updown = current_stroke;
	piezoSetRatio(ratio_inout, ratio_updown);

}

static void move_fast_and_wait(uint32_t time, uint8_t start_angle, uint8_t start_stroke, uint8_t end_angle, uint8_t end_stroke, uint32_t ms_per_step)
{
	if (time == end_move_time)
	{
		start_move_time = time;
		if (start_angle < end_angle)
		{
			angle_distance = end_angle - start_angle;
			negative_angle = 0;
		}
		else
		{
			angle_distance = start_angle - end_angle;
			negative_angle = 1;
		}
		if (start_stroke < end_stroke)
		{
			stroke_distance = end_stroke - start_stroke;
			negative_stroke = 0;
		}
		else
		{
			stroke_distance = start_stroke - end_stroke;
			negative_stroke = 1;
		}
		if (angle_distance > stroke_distance)
		{
			steps = angle_distance;
		}
		else
		{
			steps = stroke_distance;
		}
		move_ms = steps*ms_per_step;
		move_ms = move_ms - move_ms%10 + 10;
		end_move_time = start_move_time + move_ms + 10;
	}
	ratio_inout = end_angle;
	ratio_updown = end_stroke;
	piezoSetRatio(ratio_inout, ratio_updown);

}

static uint8_t start_angle = 0;
static uint8_t stop_angle = 0;
static uint8_t start_stroke = 0;
static uint8_t stop_stroke = 0;
static uint8_t initial_angle = 0;
static uint8_t initial_stroke = 0;
static uint8_t ms_step = DEFAULT_MS_PER_STEP;

static uint8_t up_stroke = 70;
static uint8_t down_stroke = 140;
static uint8_t top_contact = 135;
static uint8_t bottom_contact = 120;
static uint8_t top_slant_width = 20;
static uint8_t top_slant_height = 20;
static uint8_t bottom_slant_width = 15;
static uint8_t bottom_slant_height = 20;
static uint8_t top_horizontal_trim = 0;
static uint8_t bottom_horizontal_trim = 3;
static uint32_t vertical_speed = 1; //DEFAULT_MS_PER_STEP;
static uint32_t horizontal_speed = 8; //DEFAULT_MS_PER_STEP*2;
static uint32_t top_slant_speed = 1; //DEFAULT_MS_PER_STEP;
static uint32_t bottom_slant_speed = 1; //DEFAULT_MS_PER_STEP;

/*static uint32_t move = 0;
static void gait(uint32_t time)
{
	if(end_move_time == 0)
	{
		end_move_time = time;
		move = 1;
	}
	else if(time == end_move_time)
	{
		move++;
		start_angle = stop_angle;
		start_stroke = stop_stroke;
	}

	switch (move)
	{
	case HORIZONTAL_IN:
		initial_angle = bottom_contact - bottom_slant_width;
		initial_stroke = up_stroke;
		start_angle = initial_angle;
		start_stroke = initial_stroke;
		stop_angle = top_contact;
		stop_stroke = up_stroke + top_horizontal_trim;
		ms_step = horizontal_speed;
		move_foot(time, start_angle, start_stroke, stop_angle, stop_stroke, ms_step);
		break;
	case SLANT_IN:
		stop_angle = top_contact + top_slant_width;
		stop_stroke = up_stroke + top_slant_height;
		ms_step = top_slant_speed;
		move_foot(time, start_angle, start_stroke, stop_angle, stop_stroke, ms_step);
		break;
	case VERTICAL_DOWN:
		stop_angle = top_contact + top_slant_width;
		stop_stroke = down_stroke;
		ms_step = vertical_speed;
		move_foot(time, start_angle, start_stroke, stop_angle, stop_stroke, ms_step);
		break;
	case HORIZONTAL_OUT:
		stop_angle = bottom_contact;
		stop_stroke = down_stroke - bottom_horizontal_trim;
		ms_step = horizontal_speed;
		move_foot(time, start_angle, start_stroke, stop_angle, stop_stroke, ms_step);
		break;
	case SLANT_OUT:
		stop_angle = bottom_contact - bottom_slant_width;
		stop_stroke = down_stroke - bottom_slant_height;
		ms_step = bottom_slant_speed;
		move_foot(time, start_angle, start_stroke, stop_angle, stop_stroke, ms_step);
		break;
	case VERTICAL_UP:
		stop_angle = initial_angle;
		stop_stroke = initial_stroke;
		ms_step = vertical_speed;
		move_foot(time, start_angle, start_stroke, stop_angle, stop_stroke, ms_step);
		break;
	default:
		move = 0;
		end_move_time = time + 10;
	}

}*/

static uint8_t takeoff_angle = 100;
static uint8_t takeoff_stroke = 165;
static uint32_t takeoff_move;
static void takeoff(uint32_t time)
{
	if(end_move_time == 0)
	{
		end_move_time = time;
		takeoff_move = 1;
		start_angle = ratio_inout;
		start_stroke = ratio_updown;
	}
	else if(time == end_move_time)
	{
		takeoff_move++;
		start_angle = stop_angle;
		start_stroke = stop_stroke;
	}

	switch (takeoff_move)
	{
	case 1:
		stop_angle = bottom_contact-bottom_slant_width;
		stop_stroke = up_stroke + top_slant_height;
		move_foot(time, start_angle, start_stroke, stop_angle, stop_stroke, DEFAULT_MS_PER_STEP);
		break;
	case 2:
		stop_angle = bottom_contact-bottom_slant_width;
		stop_stroke = up_stroke;
		move_foot(time, start_angle, start_stroke, stop_angle, stop_stroke, DEFAULT_MS_PER_STEP);
		break;
	case 3:
		stop_angle = takeoff_angle;
		stop_stroke = takeoff_stroke;
		move_fast_and_wait(time, start_angle, start_stroke, stop_angle, stop_stroke, DEFAULT_MS_PER_STEP);
		break;
	default:
		takeoff_move = 0;
		end_move_time = time + 10;
	}

}

static float xid = 0;
static float zid = 0;
static float x = 0;
static float z = 0;
static float z_reference = -0.05;
static float x_reference = -1.00;
static float z_detection_offset = -.01;
static float x_detection_offset = -.1;
static uint8_t top_search_length = 25;
static uint8_t bottom_search_length = 5;
static uint32_t top_searching_speed = 1; //DEFAULT_MS_PER_STEP;
static uint32_t bottom_searching_speed = 1; //DEFAULT_MS_PER_STEP;
static uint8_t top_search_trim = 0;
static uint8_t bottom_search_trim = 0;
static uint8_t asperity_found = 0;
static uint8_t next_gait_state = 0;
static void smart_gait(uint32_t time)
{
	xid = logGetVarId("acc", "x");
	zid = logGetVarId("acc", "z");
	x = logGetFloat(xid);
	z = logGetFloat(zid);
	if(end_move_time == 0)
	{
		end_move_time = time;
		gait_state = 1;
		asperity_found = 1;
	}
	else if(time == end_move_time)
	{
		if(gait_state == RECYCLING_TOP_FOOT)
		{
			x_reference = x;
			z_reference = z;
		}
//		if((gait_state == SEARCHING_FOR_TOP_ASPERITY)&&(!asperity_found))
//		{
//			gait_state = RESET_TOP_SEARCH;
//		}
//		else
//		{
//			gait_state = next_gait_state;
//		}
		gait_state = next_gait_state;
		start_angle = stop_angle;
		start_stroke = stop_stroke;
	}
	switch (gait_state)
	{
	case MOVING_HORIZONTALLY_IN:
		initial_angle = bottom_contact - bottom_slant_width;
		initial_stroke = up_stroke;
		start_angle = initial_angle;
		start_stroke = initial_stroke;
		stop_angle = top_contact;
		stop_stroke = up_stroke + top_horizontal_trim;
		ms_step = horizontal_speed;
		move_foot(time, start_angle, start_stroke, stop_angle, stop_stroke, ms_step);
		next_gait_state = SEARCHING_FOR_TOP_ASPERITY;
		asperity_found = 0;
		break;
	case SEARCHING_FOR_TOP_ASPERITY:
		stop_angle = top_contact + top_search_trim;
		stop_stroke = up_stroke + top_horizontal_trim + top_search_length;
		if (x < (x_reference + x_detection_offset)){
			asperity_found = 1;
			end_move_time = time + 10;
			next_gait_state = ENGAGING_TOP_FOOT;
		} else {
			ms_step = top_searching_speed;
			move_fast_and_wait(time, start_angle, start_stroke, stop_angle, stop_stroke, ms_step);
			next_gait_state = RESET_TOP_SEARCH;
		}
		break;
	case ENGAGING_TOP_FOOT:
		stop_angle = top_contact + top_slant_width;
		stop_stroke = up_stroke + top_horizontal_trim + top_slant_height + top_search_length;
		ms_step = top_slant_speed;
		move_fast_and_wait(time, start_angle, start_stroke, stop_angle, stop_stroke, ms_step);
		next_gait_state = PULLING_UP;
		break;
	case PULLING_UP:
		stop_angle = top_contact + top_slant_width;
		stop_stroke = down_stroke;
		ms_step = vertical_speed;
		move_fast_and_wait(time, start_angle, start_stroke, stop_angle, stop_stroke, ms_step);
		next_gait_state = MOVING_HORIZONTALLY_OUT;
		break;
	case MOVING_HORIZONTALLY_OUT:
		stop_angle = bottom_contact;
		stop_stroke = down_stroke  - bottom_horizontal_trim;
		ms_step = horizontal_speed;
		move_foot(time, start_angle, start_stroke, stop_angle, stop_stroke, ms_step);
		next_gait_state = SEARCHING_FOR_BOTTOM_ASPERITY;
		break;
	case SEARCHING_FOR_BOTTOM_ASPERITY:
		stop_angle = bottom_contact - bottom_search_trim;
		stop_stroke = down_stroke - bottom_horizontal_trim - bottom_search_length;
		ms_step = bottom_searching_speed;
		move_fast_and_wait(time, start_angle, start_stroke, stop_angle, stop_stroke, ms_step);
		next_gait_state = ENGAGING_BOTTOM_FOOT;
		break;
	case ENGAGING_BOTTOM_FOOT:
		stop_angle = bottom_contact - bottom_slant_width;
		stop_stroke = down_stroke  - bottom_horizontal_trim - bottom_search_length - bottom_slant_height;
		ms_step = bottom_slant_speed;
		move_fast_and_wait(time, start_angle, start_stroke, stop_angle, stop_stroke, ms_step);
		next_gait_state = RECYCLING_TOP_FOOT;
		break;
	case RECYCLING_TOP_FOOT:
		stop_angle = initial_angle;
		stop_stroke = initial_stroke;
		ms_step = vertical_speed;
		move_fast_and_wait(time, start_angle, start_stroke, stop_angle, stop_stroke, ms_step);
		next_gait_state = MOVING_HORIZONTALLY_IN;
		break;
	case RESET_TOP_SEARCH:
		stop_angle = initial_angle;
		stop_stroke = initial_stroke;
		ms_step = vertical_speed;
		move_fast_and_wait(time, start_angle, start_stroke, stop_angle, stop_stroke, ms_step);
		break;
	default:
		gait_state = STAYING_NEUTRAL;
		end_move_time = time + 10;
	}

}

uint8_t static_ratio1 = 0;
uint8_t static_ratio2 = 0;
static int state = FLYING;
static int stateid;
static int old_state = FLYING;
static void bypass(uint32_t counter)
{
  /* Just set the params from the host */
	stateid = logGetVarId("stateMachine", "state");
	state = logGetInt(stateid);
	piezoSetFreq(static_freq);

	if(state!=old_state)
	{
		end_move_time = 0;
	}

	switch (state)
	{
	case FLYING:
		ratio_inout = static_ratio1;
		ratio_updown = static_ratio2;
		piezoSetRatio(ratio_inout, ratio_updown);
		break;
	case CLIMBING:
		smart_gait(counter);
		break;
	case TAKEOFF:
		takeoff(counter);
		//takeoff(counter);
		break;
	case RESET_SERVOS:
		ratio_inout = FULL_OUT;
		ratio_updown = FULL_UP;
		piezoSetRatio(ratio_inout, ratio_updown);
		static_ratio1 = 0;
		static_ratio2 = 0;
		break;
	default:
		piezoSetRatio(0, 0);
	}

	old_state = state;
}

uint16_t siren_start = 2000;
uint16_t siren_freq = 2000;
uint16_t siren_stop = 4000;
int16_t siren_step = 40;
static void siren(uint32_t counter)
{
  siren_freq += siren_step;
  if (siren_freq > siren_stop)
  {
    siren_step *= -1;
    siren_freq = siren_stop;
  }
  if (siren_freq < siren_start)
  {
    siren_step *= -1;
    siren_freq = siren_start;
  }
  //piezoSetRatio(127);
  piezoSetFreq(siren_freq);
}


static int pitchid;
static int rollid;
static int pitch;
static int roll;
static int tilt_freq;
static int tilt_ratio;
static void tilt(uint32_t counter)
{
  pitchid = logGetVarId("stabilizer", "pitch");
  rollid = logGetVarId("stabilizer", "roll");

  pitch = logGetInt(pitchid);
  roll = logGetInt(rollid);
  tilt_freq = 0;
  tilt_ratio = 127;

  if (abs(pitch) > 5) {
    tilt_freq = 3000 - 50 * pitch;
  }

  //piezoSetRatio(tilt_ratio);
  piezoSetFreq(tilt_freq);
}


unsigned int neffect = 0;
unsigned int effect = 0;
BuzzerEffect effects[] = {bypass, siren, melodyplayer, tilt};

static xTimerHandle timer;
static uint32_t counter = 0;

static void buzzTimer(xTimerHandle timer)
{
  counter++;

  if (effects[effect] != 0)
    effects[effect](counter*10);
}

void buzzerInit(void)
{
  piezoInit();

  neffect = sizeof(effects)/sizeof(effects[0])-1;
  nmelody = sizeof(melodies)/sizeof(melodies[0])-1;

  timer = xTimerCreate( (const signed char *)"buzztimer", M2T(10),
                                     pdTRUE, NULL, buzzTimer );
  xTimerStart(timer, 100);
}

LOG_GROUP_START(servo_status)
LOG_ADD(LOG_UINT8, ratio_inout, &ratio_inout)
LOG_ADD(LOG_UINT8, ratio_updown, &ratio_updown)
LOG_GROUP_STOP(servo_status)

LOG_GROUP_START(sensing)
LOG_ADD(LOG_FLOAT, x_ref, &x_reference)
LOG_ADD(LOG_FLOAT, z_ref, &z_reference)
LOG_GROUP_STOP(sensing)


PARAM_GROUP_START(buzzer)
//PARAM_ADD(PARAM_UINT8, effect, &effect)
//PARAM_ADD(PARAM_UINT32 | PARAM_RONLY, neffect, &neffect)
//PARAM_ADD(PARAM_UINT8, melody, &melody)
//PARAM_ADD(PARAM_UINT32 | PARAM_RONLY, nmelody, &melody)
PARAM_ADD(PARAM_UINT16, freq, &static_freq)
PARAM_ADD(PARAM_UINT8, ratio1, &static_ratio1)
PARAM_ADD(PARAM_UINT8, ratio2, &static_ratio2)
PARAM_GROUP_STOP(buzzer)

PARAM_GROUP_START(takeoff)
PARAM_ADD(PARAM_UINT8, takeoff_angle, &takeoff_angle)
PARAM_ADD(PARAM_UINT8, takeoff_stroke, &takeoff_stroke)
PARAM_GROUP_STOP(takeoff)

PARAM_GROUP_START(limits)
PARAM_ADD(PARAM_UINT8, up_stroke, &up_stroke)
PARAM_ADD(PARAM_UINT8, down_stroke, &down_stroke)
PARAM_ADD(PARAM_UINT8, top_contact, &top_contact)
PARAM_ADD(PARAM_UINT8, bottom_contact, &bottom_contact)
PARAM_GROUP_STOP(limits)

PARAM_GROUP_START(speeds)
PARAM_ADD(PARAM_UINT32, vertical_speed, &vertical_speed)
PARAM_ADD(PARAM_UINT32, horizontal_speed, &horizontal_speed)
PARAM_ADD(PARAM_UINT32, top_slant_spd, &top_slant_speed)
PARAM_ADD(PARAM_UINT32, bottom_slant_spd, &bottom_slant_speed)
PARAM_ADD(PARAM_UINT32, top_search_spd, &top_searching_speed)
PARAM_ADD(PARAM_UINT32, bottom_search_spd, &bottom_searching_speed)
PARAM_GROUP_STOP(speeds)

PARAM_GROUP_START(tuning)
PARAM_ADD(PARAM_UINT8, top_slant_w, &top_slant_width)
PARAM_ADD(PARAM_UINT8, top_slant_h, &top_slant_height)
PARAM_ADD(PARAM_UINT8, bottom_slant_w, &bottom_slant_width)
PARAM_ADD(PARAM_UINT8, bottom_slant_h, &bottom_slant_height)
PARAM_ADD(PARAM_UINT8, top_h_trim, &top_horizontal_trim)
PARAM_ADD(PARAM_UINT8, bottom_h_trim, &bottom_horizontal_trim)
PARAM_ADD(PARAM_UINT8, top_s_trim, &top_search_trim)
PARAM_ADD(PARAM_UINT8, bottom_s_trim, &bottom_search_trim)
PARAM_ADD(PARAM_UINT8, top_s_len, &top_search_length)
PARAM_ADD(PARAM_UINT8, bottom_s_len, &bottom_search_length)
PARAM_GROUP_STOP(tuning)

PARAM_GROUP_START(sensing)
PARAM_ADD(PARAM_FLOAT, x_detect_o, &x_detection_offset)
PARAM_ADD(PARAM_FLOAT, z_detect_o, &z_detection_offset)
PARAM_GROUP_STOP(sensing)
