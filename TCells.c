/*
 * TCells.c
 *
 *  Created on: 23 Aug 2018
 *      Author: Parhizkar
 https://www.cgl.ucsf.edu/chimera/data/kilobots-jan2015/cart.c
 */


#include "kilolib.h"

// Must define ME to be one of the following cell types
// #define ME T1

// T-cell intracellular domains: weak=T1, medium=T2, strong=T3, nucleus=TN
#define T1 1
#define T2 2
#define T3 3
#define TN 11

// 3 antibodies, one binds cancer antigen only, one healthy antigen only, one binds both
#define ABC 4
#define ABH 5
#define AB2 6

// Antigens for cancer and healthy cell
#define AGC 7
#define AGH 8

// Cancer and health cell interior molecules
#define CI 9
#define HI 10

#define DISARM_TICKS 64		// Separation time that causes disarming of t-cell
#define DEATH_TICKS 192		// How long it takes cell to die.
#define ANTIBODY_ANTIGEN_MAX_DISTANCE_MM 75	// Max distance for antibody/antigen binding, millimeters

uint8_t kill, dead;
uint32_t last_t_ab_time, last_ab_ag_time, last_strength_time, kill_time;
message_t msg;

uint8_t is_tcell(uint8_t t)
 { return ((t == T1 || t == T2 || t == T3) ? 1 : 0); }
uint8_t is_antibody(uint8_t t)
 { return ((t == ABC || t == ABH || t == AB2) ? 1 : 0); }
uint8_t is_antigen(uint8_t t)
 { return ((t == AGC || t == AGH) ? 1 : 0); }
uint8_t is_target_cell(uint8_t t)
 { return ((t == CI || t == HI) ? 1 : 0); }
uint8_t compatible_antibody_antigen(t1, t2)
{
  return ((t1 == ABC && t2 == AGC) ||
	  (t1 == ABH && t2 == AGH) ||
	  (t1 == AB2 && (t2 == AGC || t2 == AGH))
	  ? 1 : 0);
}
uint8_t is_t_armed()
{
  return (kilo_ticks < last_t_ab_time + DISARM_TICKS &&
	  kilo_ticks < last_ab_ag_time + DISARM_TICKS);
}
uint8_t is_t_ready()
{
  return (kilo_ticks < last_t_ab_time + DISARM_TICKS);
}

// turn flag on message reception
void message_rx(message_t *m, distance_measurement_t *d) {
  uint8_t m0 = m->data[0];

  if ((is_tcell(m0) && is_antibody(ME)) || (is_tcell(ME) && is_antibody(m0)))
    last_t_ab_time = kilo_ticks;
  else if (kilo_ticks > last_t_ab_time + m->data[1])
    last_t_ab_time = kilo_ticks - m->data[1];

  if ((compatible_antibody_antigen(m0, ME) ||
       compatible_antibody_antigen(ME, m0))
      && estimate_distance(d) < ANTIBODY_ANTIGEN_MAX_DISTANCE_MM)
    last_ab_ag_time = kilo_ticks;
  else if (kilo_ticks > last_ab_ag_time + m->data[2])
    last_ab_ag_time = kilo_ticks - m->data[2];

  if (!is_tcell(ME) && last_strength_time + m->data[3] < kilo_ticks)
    {
      last_strength_time = kilo_ticks - m->data[3];
      msg.data[4] = m->data[4];
    }

  if (!kill && is_target_cell(ME) && is_t_armed())
    {
      kill = 1;
      kill_time = kilo_ticks;
    }
}

message_t *message_tx() {
  uint32_t delay = kilo_ticks - last_t_ab_time;
  msg.data[1] = (delay > 255 ? 255 : delay);
  delay = kilo_ticks - last_ab_ag_time;
  msg.data[2] = (delay > 255 ? 255 : delay);
  delay = (is_tcell(ME) ? 0 : kilo_ticks - last_strength_time);
  msg.data[3] = (delay > 255 ? 255 : delay);
  msg.crc = message_crc(&msg);
  return &msg;
}

uint8_t color_cycle(c0,t0,c1,t1) {
  return ((kilo_ticks % (t0 + t1)) < t0 ? c0 : c1);
}
uint8_t random_color(cmask,t)
{
  static uint32_t ctime = 0;
  static uint8_t rc = 0xff;
  if (kilo_ticks >= ctime + t)
    {
      rc = rand_soft();
      ctime = kilo_ticks;
    }
  return (rc&cmask);
}

void run_motors(uint8_t mleft, uint8_t mright)
{
  static uint8_t ml = 0, mr = 0;

  if ((ml == 0 && mleft != 0) || (mr == 0 && mright != 0))
    spinup_motors();
  if (mleft != ml || mright != mr)
    {
      set_motors(mleft,mright);
      ml = mleft;
      mr = mright;
    }
}

void update_state()
{
  if (kill)
    {
      if (!is_t_armed())
	{
	  kill = 0;	  // Kill signal was removed.  Bring CC back to life.
	  dead = 0;
	}
      else if (!dead && kilo_ticks >= kill_time + DEATH_TICKS &&
	       (msg.data[4] == T2 || msg.data[4] == T3))
	dead = 1;
    }
}

uint8_t color()
{
  uint8_t c;
  uint8_t a = is_t_armed(), r = is_t_ready();
  if (is_tcell(ME)) c = (r ? color_cycle(RGB(3,1,0), 8, RGB(0,0,0), 8) : RGB(3,1,0));
  else if (ME == TN) c = (r ? color_cycle(RGB(0,0,1), 16, RGB(0,0,3), 16) : RGB(0,0,0));
  else if (is_antibody(ME)) c = (r ? color_cycle(RGB(0,1,0), 8, RGB(0,0,0), 8) : RGB(0,1,0));
  else if (ME == AGC) c = (a ? color_cycle(RGB(1,0,1), 8, RGB(0,0,0), 8) : RGB(1,0,1));
  else if (ME == AGH) c = (a ? color_cycle(RGB(1,1,1), 8, RGB(0,0,0), 8) : RGB(1,1,1));
  else if (ME == CI || ME == HI)
    {
      if (!kill)
	c = (ME == CI ?
	     color_cycle(RGB(1,0,0), 32, RGB(2,0,0), 32) :
	     color_cycle(RGB(2,1,1), 32, RGB(3,1,1), 32));
      else if (dead) c = RGB(0,0,0);
      else if (msg.data[4] == 1 || kilo_ticks <= kill_time + DEATH_TICKS/2) c = random_color(0x7,3);
      else c = color_cycle(RGB(3,0,0), 4, RGB(0,0,0), 4);
    }
  else
    c = RGB(3,3,3);
  return c;
}

uint8_t motor_speed()
{
  uint8_t s = 0;
  if (kill && !dead)
    {
      // Death sequence.
      if (msg.data[4] == T2)
	s = 60;
      else if (msg.data[4] == T3)
	s = 130;
    }
  return s;
}

void setup() {
    // initialize message
    msg.type = NORMAL;
    msg.data[0] = ME;
    msg.data[1] = 255;	// Time in ticks since Tn was near ABx.
    msg.data[2] = 255;	// Time in ticks since ABx was near AGy.
    msg.data[3] = (is_tcell(ME) ? 0 : 255);	// Time in ticks since last strength report.
    msg.data[4] = (is_tcell(ME) ? ME : 0);	// T-cell response strength.
    msg.crc = message_crc(&msg);
    last_t_ab_time = 0;
    last_ab_ag_time = 0;
    last_strength_time = 0;
    kill = 0;
    dead = 0;
    rand_seed(rand_hard());
}

void loop() {
  update_state();
  set_color(color());
  uint8_t m = motor_speed();
  run_motors(m,m);
}

int main() {
    // initialize hardware
    kilo_init();
    // register message reception callback
    kilo_message_rx = message_rx;
    // register message_tx function
    kilo_message_tx = message_tx;
    // register your program
    kilo_start(setup, loop);

    return 0;
}
