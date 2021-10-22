/******************************************************************************
 *
 * File:             mdeGranular~.c
 *
 * Author:           Michael Edwards - m@michael-edwards.org
 *                   http://www.michael-edwards.org
 *
 * Date:             June 13th 2003
 *
 * $$ Last modified:  17:53:34 Tue Aug 10 2021 CEST
 *
 * Purpose:          MAX/MSP and/or PD external for multi-channel, multi-voice,
 *                   multi-transposition granular synthesis.
 *                  
 * License:          Copyright (c) 2003 Michael Edwards
 *
 *                   This file is part of mdeGranular~
 *
 *                   mdeGranular~ is free software; you can redistribute it
 *                   and/or modify it under the terms of the GNU General
 *                   Public License as published by the Free Software
 *                   Foundation; either version 2 of the License, or (at your
 *                   option) any later version.
 *
 *                   mdeGranular~ is distributed in the hope that it will be
 *                   useful, but WITHOUT ANY WARRANTY; without even the
 *                   implied warranty of MERCHANTABILITY or FITNESS FOR A
 *                   PARTICULAR PURPOSE.  See the GNU General Public License
 *                   for more details.
 *
 *                   You should have received a copy of the <a
 *                   href="../../COPYING.TXT">GNU General Public License</a>
 *                   along with mdeGranular~; if not, write to the Free
 *                   Software Foundation, Inc., 59 Temple Place, Suite 330,
 *                   Boston, MA 02111-1307 USA
 *
 *****************************************************************************/

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <float.h>
#include <ctype.h>
#include "mdeGranular~.h"

/*****************************************************************************/

/** If DEBUG is #defined then details of each grain and its samples will be
 *  written to a separate file in the temp directory.  N.B. These will only be
 *  meaningful however if the number of voices is 1.

#define DEBUG 1
 * */

#ifdef DEBUG
static FILE* DebugFP = NULL;
#endif

/*****************************************************************************/

/** The mdeGranular object's set methods: */

/*****************************************************************************/

/**  -activeVoices- is only a float because this is the type we get from MAX/PD 
 *  */

void mdeGranularSetActiveVoices(mdeGranular* g, mdefloat activeVoices)
{
  int av = (int)activeVoices;
  int i;

  if (av >= 0 && av <= g->maxVoices) {
    g->activeVoices = av;
    if (g->grains)
      for (i = 0; i < g->maxVoices; ++i) {
        g->grains[i].activeStatus = (i >= av ? INACTIVE : ACTIVE);
        g->grains[i].doDelay = 1;
      }
  }
  else if (g->warnings) {
    post("mdeGranular~:");
    post("              argument %d is invalid for active voices",
         (int)activeVoices);
    post("              (max voices = %d)", g->maxVoices);
  }
}

/*****************************************************************************/

/** -maxVoices- is only a float because this is the type we get from PD 
 *  */

void mdeGranularSetMaxVoices(mdeGranular* g, mdefloat maxVoices)
{
  int mv = (int)maxVoices;

  if (mv > 0) {
    g->maxVoices = mv;
    if (g->grains)
      mdeFree(g->grains);
    g->grains = mdeCalloc(mv, sizeof(mdeGranularGrain), 
                          "mdeGranularSetMaxVoices", g->warnings);
    if (mv < g->activeVoices)
      g->activeVoices = mv;
    mdeGranularSetActiveVoices(g, (mdefloat)g->activeVoices);
    /* init each grain/voice */
    mdeGranularInitGrains(g);
  }
}

/*****************************************************************************/

void mdeGranularSetRampType(mdeGranular* g, char* type)
{
  /* 10.9.10: don't change the ramp unless we're off and therefore not using
   * the ramp array */
  if (g->status != OFF) {
    if (g->warnings) {
      post("mdeGranular~:");
      post("              Can't change ramp type whilst object is running ");
      post("              or ramping down. Ignoring.");
    }
    return;
  }
  mdeGranularStoreRampType(g, type);
  mdeGranularInitGrains(g);
  makeWindow(g->rampType, g->rampLenSamples * 2, 2.5, g->rampUp);
}

/*****************************************************************************/

void mdeGranularSetRampLenMS(mdeGranular* g, mdefloat rampLenMS)
{
  mdefloat halfgrainlength = g->grainLengthMS * (mdefloat)0.5;
  mdefloat* oldramp = g->rampUp;
  /* post("\nSETRAMPLENMS: %fms (srate=%f)", rampLenMS, g->samplingRate); 
     return;  */

  /* 10.9.10: don't change the ramp unless we're off and therefore not using
   * the ramp array */
  if (g->status != OFF) {
    if (g->warnings) {
      post("mdeGranular~:");
      post("              Can't change ramp length whilst object is running ");
      post("              or ramping down. Ignoring.");
    }
    return;
  }
  if (rampLenMS < (mdefloat)RAMPLENMINMS) {
    if (g->warnings)
      post("mdeGranular~: Ramp Length (%fms) too small, setting to min.: %fms",
           rampLenMS, RAMPLENMINMS);
    rampLenMS = (mdefloat)RAMPLENMINMS;
  }
  if (rampLenMS > halfgrainlength) {
    if (g->warnings) {
      post("mdeGranular~:");
      post("              Ramp Length (%f) must be a maximum of half ",
           rampLenMS);
      post("              the grain length (%f, half = %f).",
           g->grainLengthMS, halfgrainlength);
      post("              Ignoring.");
    }
    return;
  }
  g->rampLenMS = rampLenMS;
  g->rampLenSamples = ms2samples(g->samplingRate, g->rampLenMS);
  /* the ramp up/down is in fact one contiguous block with the down being
   * simply a pointer to the middle */
  /* todo: don't malloc here if we're using the ramp: in fact only allow ramps
     to be changed when object is off */ 
  g->rampUp = mdeCalloc(g->rampLenSamples * 2, sizeof(mdefloat),
                        "mdeGranularSetRampLenMS", g->warnings);
  g->rampDown = g->rampUp + g->rampLenSamples;
  /* remember: the 2.5 is CLM's mysterious 'beta' arg... */
  makeWindow(g->rampType, g->rampLenSamples * 2, 2.5, g->rampUp);
  /* reinitialize the grains now we have a different ramp length--this will
   * only happen if we have samples already so should be ignored if we're at
   * the init stage */
  mdeGranularInitGrains(g);
  if (oldramp)
    mdeFree(oldramp);
  /* post("\nSETRAMPLENMS: now %f", g->rampLenMS); */
}

/*****************************************************************************/

void mdeGranularSetTranspositionOffsetST(mdeGranular* g, mdefloat f)
{
  /* f is semitones */
  g->transpositionOffsetST = f;
  g->transpositionOffset = st2src(f, g->octaveSize, g->octaveDivisions);
}

/*****************************************************************************/

/** This does the actual memory allocation for live granulation (i.e. not
 *  the 'set ms500' message--that's handled in init3.  Should generally only be
 *  called once per performance unless you're really careful.  */

void mdeGranularSetLiveBufferSize(mdeGranular* g, mdefloat sizeMS)
{
  /* 7/3/06: only malloc if we're off and therefore not accessing previously
     allocated memory! */
  if (g->status == OFF) {
    /* 3.4.10 don't allow us to set a max buffer size < the grain
       length--without checking we'd have a crash! */  
    if (g->grainLengthMS > sizeMS && g->warnings) {
      post("mdeGranular~:");
      post("              Can't change maximim buffer size to %f as your ",
           sizeMS);
      post("              grain length is %f (i.e. larger).  Ignoring.",
           g->BufferSamplesMS);
    }
    else {
      int numSamples = ms2samples(g->samplingRate, sizeMS);
      mdefloat* old = g->theSamples;
      g->theSamples = mdeCalloc(numSamples, sizeof(mdefloat),
                                 "mdeGranularSetLiveBufferSize", g->warnings);
      g->nAllocatedBufferSamples = numSamples;
      g->AllocatedBufferMS = sizeMS;
      if (g->live)
        g->samples = g->theSamples;
      if (old)
        mdeFree(old);
    }
  }
  else if (g->warnings) {
    post("mdeGranular~:");
    post("              Can't change buffer size while object is running ");
    post("              (or ramping down)!");
  }
}

/*****************************************************************************/

void mdeGranularOctaveSize(mdeGranular* g, mdefloat size)
{
  if (size > 0.0)
    g->octaveSize = size;
  else if (g->warnings)
    post("mdeGranular~: OctaveSize must be > 0: %f!", size);
}

/*****************************************************************************/

void mdeGranularOctaveDivisions(mdeGranular* g, mdefloat divs)
{
  if (divs > 0.0)
    g->octaveDivisions = divs;
  else if (g->warnings)
    post("mdeGranular~: OctaveDivisions must be > 0: %f!", divs);
}

/*****************************************************************************/

mdefloat maxFloat(mdefloat* array, int size)
{
  int i;
  /* MDE Thu Sep 19 19:40:48 2013 -- bear in mind that this used to be FLT_MIN
   * but had to be updated when we switched from 32 to 64 bit floats. Both are
   * defined in float.h */
  mdefloat result = (mdefloat)DBL_MIN;
  mdefloat tmp;

  for (i = 0; i < size; ++i) {
    tmp = *array++;
    if (tmp > result)
      result = tmp;
  }
  return result;
}

/*****************************************************************************/

void mdeGranularSetGrainLengthMS(mdeGranular* g, mdefloat f)
{
  mdefloat sr = g->samplingRate;
  int lenSamples = ms2samples(sr, f);
  mdefloat highestSRC = maxFloat(g->srcs, g->numTranspositions);
  int sampsNeeded = lenSamples * highestSRC * g->transpositionOffset;

  if (f <= (2 * g->rampLenMS)) {
    if (g->warnings) {
      post("mdeGranular~:");
      post("              grain length (%f) too small for ", f);
      post("              given ramp length (%f).", g->rampLenMS);
      post("              Ignoring.");
    }
    return;
  }
  /*
   * 1/5/08: we used to demand that we had twice as many samples in the buffer
   * as we need.  */
  /*
    if (g->nBufferSamples && sampsNeeded >= (g->nBufferSamples / 2)) {
  */
  if (g->nBufferSamples && sampsNeeded >= g->nBufferSamples) {
    mdefloat msneeded = samples2ms(sr, sampsNeeded);
    if (g->warnings) {
      post("mdeGranular~:");
      post("              Live (internal) sample buffer is too short for ");
      post("              requested grain length with given transpositions.");
      post("              Buffer should generally be twice the grain length.");
      post("              (Use the 'set msXXX' message to set the internal ");
      post("              buffer size in millisecs.)");
      post("              (%d samples (%fms) in buffer, ",
           g->nBufferSamples, samples2ms(sr, g->nBufferSamples));
      post("              %d (%fms) samples in grain, ",
           lenSamples, samples2ms(sr, lenSamples));
      post("              %d (%fms) samples needed for highest transposition)",
           sampsNeeded, msneeded);
      post("              Min buffer size should be %f",
           msneeded * 2.0f);
      post("              Ignoring.");
    }
    return;
  }
  g->grainLengthMS = f;
  g->grainLength = lenSamples;
  /* now we have a different grain length (which could be suddenly
   * much longer) we should have a delay the next time the grain is
   * initialised */
  /* todo: This would screw up loops of dynamically changing lengths
   * though so don't do it for now, rather rethink this (structure
   * member switch via message?). */
  /*
    mdeGranularDoGrainDelays(g);
  */
}

/*****************************************************************************/

void mdeGranularDoGrainDelays(mdeGranular* g)
{
  int i;
  
  if (g->grains)
    for (i = 0; i < g->maxVoices; ++i) {
      g->grains[i].doDelay = 1;
    }
}

/*****************************************************************************/

/** Spread out the grains evenly (and with no grain length deviation)
 *  NB if ActiveVoices is changed, this will not retrigger this
 *  function
 *  May cause click in output so it is envisaged that the fader is down
 */

void mdeGranularSmoothMode(mdeGranular* g)
{
  int i;
  int av = g->activeVoices;
  int len = g->grainLength;
  int delay = 0;
  int dinc = len / av;

  /* stop all grains now--might cause click! */
  mdeGranularForceGrainReinit(g);
  /* if 100 voices and 1ms grains at 44.1kHz this would mean a dinc of 0 which
   * means no delay; plus a doDelay of 1 means randomly vary the delay, so
   * limit to a minimum of 2 samples.
   */
  if (dinc < 2)
    dinc = 2;
  g->grainLengthDeviation = (mdefloat)0.0;
  if (g->grains)
    for (i = 0; i < av; ++i) {
      g->grains[i].doDelay = delay;
      delay += dinc;
    }
}

/*****************************************************************************/

/* 1.8.10: We no longer need this.
 * The original algorithm chooses a grain from anywhere within the current
 * sample buffer. By turning on Focus Mode we can make sure each grain starts
 * at a current read pointer. This starts at the beginning of the buffer (or
 * the user-given start point) and increments 'speed' samples for each sample
 * output (obviously if we're live granulating a 'speed' higher than 1 would be
 * potentially problematic (though we won't disallow it).

 * To use the correct read pointer we increment it once only by an audio tick
 * ('tickSize') multiplied by 'speed' in mdeGranularGo then, in GrainMixIn,
 * where we call mdeGranularGrainInit we use the read pointer plus i*speed as
 * the start point. This will mean we need a new field in mdeGranular to hold
 * the current read pointer, and another argument to GrainInit to specify where
 * we should start the grain. We also need to detect when we're at the end of
 * the sample buffer. We don't need to worry about whether we're live or not
 * as GrainInit handles this simply by incrementing calculated start points by
 * the current input write pointer.

 * The other thing to be wary of, of course, is what we do when we get to the
 * end of the buffer. Best plan is probably to start going backwards if we
 * have a static buffer, or jump back to the beginning (i.e. continue) if we're
 * live. 

 * The best way to handle all this would be simply to manage the buffer start
 * and end points (not overwriting these user-given data of course) using the
 * Focus Mode data, and let GrainInit handle things normally. FocusMode should
 * take a 'slack' argument which is a multiplier of grain length: this will set
 * how much room GrainInit will have to select the start point.

 * SmoothMode is a side effect here, in order to avoid just duplicating the
 * same grain (i.e. we want them spread out)


void mdeGranularFocusMode(mdeGranular* g, mdefloat speed, mdefloat slack)
{

}
 */
/*****************************************************************************/

void mdeGranularSetGrainLengthDeviation(mdeGranular* g, mdefloat f)
{
  if (f >= (mdefloat)0.0 && f <= (mdefloat)100.0)
    g->grainLengthDeviation = f;
}

/*****************************************************************************/

/* 1.8.10: Here we set the start/end points in the buffer by passing values in
 * percentages: the position will set the middle point between start and end,
 * as determined by width.
 */
void mdeGranularPortion(mdeGranular* g, mdefloat position, 
                               mdefloat width)
{
  if (width <= (mdefloat)0.0 || width > (mdefloat)100.0 ||
      position < (mdefloat)0.0 || position > (mdefloat)100.0) {
    if (g->warnings) {
      post("mdeGranular~:");
      post("              mdeGranularPortion: position and width are in ");
      post("              percentages so >= 0 and <= 100.");
      post("              (position = %f, width = %f).");
      post("              Ignoring.");
    }
  }
  else {
    mdefloat buf_ms = g->BufferSamplesMS;
    mdefloat width_ms = buf_ms * width * (mdefloat)0.01;
    mdefloat half_width_ms = width_ms * (mdefloat)0.5;
    mdefloat pos_ms = buf_ms * position * (mdefloat).01;
    mdefloat start = pos_ms - half_width_ms;
    mdefloat end = pos_ms + half_width_ms;
    g->portionPosition = position;
    g->portionWidth = width;
    if (start < (mdefloat)0) {
      start = (mdefloat)0;
      end = width_ms;
    }
    /* it's feasible that this would still result in start < 0 but that will be
     * picked up and dealt with later on */  
    if (end > buf_ms) {
      end = buf_ms;
      start = buf_ms - width_ms;
    }
    mdeGranularSetSamplesStartMS(g, start);
    mdeGranularSetSamplesEndMS(g, end);
  }
}

/*****************************************************************************/

/* 31/8/10 similar to Portion, here we just set one of the parameters, using
 * the previously stored value for the other
 */
void mdeGranularPortionPosition(mdeGranular* g, mdefloat position)
{
  mdeGranularPortion(g, position, g->portionWidth);
}

void mdeGranularPortionWidth(mdeGranular* g, mdefloat width)
{
  mdeGranularPortion(g, g->portionPosition, width);
}

/*****************************************************************************/

void mdeGranularSetSamplesStartMS(mdeGranular* g, mdefloat f)
{
  g->samplesStartMS = f;
  g->samplesStart = ms2samples(g->samplingRate, f);
  if (g->samplesStart < 0) {
    g->samplesStart = 0;
    g->samplesStartMS = samples2ms(g->samplingRate, g->samplesStart);
    if ((f != (mdefloat)DBL_MIN) && (f != (mdefloat)0.0) && g->warnings) {
      post("mdeGranular~:");
      post("              %fms is too low for start point in buffer ", f);
      post("              Setting to %fms", g->samplesStartMS);
    }
  }
  if (g->samplesStart >= g->nBufferSamples) {
    g->samplesStart = g->nBufferSamples - 1;
    g->samplesStartMS = samples2ms(g->samplingRate, g->samplesStart);
    if ((f != g->BufferSamplesMS) && g->warnings) {
      post("mdeGranular~: ");
      post("              %fms is too high for start point in buffer.", f);
      post("              Setting to %fms", g->samplesStartMS);
    }
  }
}

/*****************************************************************************/

void mdeGranularSetSamplesEndMS(mdeGranular* g, mdefloat f)
{
  if (!g->nBufferSamples)
    error("mdeGranular~: No samples in buffer %s", g->BufferName);
  g->samplesEndMS = f;
  g->samplesEnd = ms2samples(g->samplingRate, f);
  /* at init we call this function with DBL_MIN to trigger this clause */
  if (g->samplesEnd >= g->nBufferSamples ||
      g->samplesEndMS == (mdefloat)DBL_MIN) {
    /* we take samples away as we need these for 4-point interpolating
     * lookup. */  
    g->samplesEnd = g->nBufferSamples - 1;
    g->samplesEndMS = samples2ms(g->samplingRate, g->samplesEnd);
    if ((f != (mdefloat)DBL_MIN) && (f != g->BufferSamplesMS) && g->warnings) {
      post("mdeGranular~:");
      post("              %fms is too high for end point in buffer (%s: %f)",
           f, g->BufferName, g->BufferSamplesMS);
      post("              Setting to %fms", g->samplesEndMS);
    }
  }
  if (g->samplesEnd < 0) {
    g->samplesEnd = 0;
    g->samplesEndMS = samples2ms(g->samplingRate, g->samplesEnd);
    if (g->warnings) {
      post("mdeGranular~:");
      post("              %fms is too low for end point in buffer.", f);
      post("              Setting to %fms", g->samplesEndMS);
    }
  }
}

/*****************************************************************************/

void warnGrain2BufferLength(mdeGranular* g)
{
  mdefloat availableBuffer = fabs(g->samplesEndMS - g->samplesStartMS);
  
  if ((availableBuffer < g->grainLengthMS) && g->warnings) {
    post("mdeGranular~:");
    post("                you are using only %fms of your buffer but have a ",
         availableBuffer);
    post("                grain length of %fms so no grains can be output.",
         g->grainLengthMS);
    post("                Note that if you are using upwards transposition");
    post("                you will need more of your buffer for a given grain");
    post("                length before output can be heard.");
  }
}

/*****************************************************************************/

void mdeGranularSetDensity(mdeGranular* g, mdefloat f)
{
  /* post("density %f", f); */
  if (f >= (mdefloat)0.0 && f <= (mdefloat)100.0)
    g->density = f;
}

/*****************************************************************************/

void mdeGranularSetActiveChannels(mdeGranular* g, long l)
{
  if (l > g->numChannels) {
    if (g->warnings) {
      post("mdeGranular~:");
      post("              ActiveChannels (%d) cannot be greater than the ",
           l);
      post("              number of outlet channels (%d).", g->numChannels);
      post("              Setting to %d.", g->numChannels);
    }
    g->activeChannels = g->numChannels;
  }
  else if (l < 1) {
    if (g->warnings) {
      post("mdeGranular~: ");
      post("              ActiveChannels (%d) cannot be less than 1. ", l);
      post("              Setting to 1.");
    }
    g->activeChannels = 1;
  }
  else g->activeChannels = l;
}

/*****************************************************************************/

void mdeGranularSetWarnings(mdeGranular* g, long l)
{
  if (l == 0 || l == 1)
    g->warnings = (char)l;
  else post("mdegranular~: Warnings should be 1 or 0.");
}
/*****************************************************************************/

void mdeGranularSetGrainAmp(mdeGranular* g, mdefloat f)
{
  static const mdefloat min = (mdefloat)0.00001;

  /* post("grainAmp %f", f); */
  if (g && f >= (mdefloat)0.0 && f <= (mdefloat)100.0) {
    if (mdeGranularAtTargetGrainAmp(g) &&
        (fabs(g->targetGrainAmp - f) > min)) {
      g->lastGrainAmp = g->grainAmp;
      g->targetGrainAmp = f < min ? (mdefloat)0.0 : f;
      g->grainAmpInc = (f - g->lastGrainAmp) / (g->nOutputSamples - 1);
    }
    /* no else clause so new values rejected unless we reached the target */
  }
}

/*****************************************************************************/

/** The list of transpositions */

void mdeGranularSetTranspositions(mdeGranular* g, int num, mdefloat* list)
{
  int i;
  mdefloat st;
  mdefloat noTransp[] = { (mdefloat)0.0 };

  /* this should only happen at init! */
  if (num == 0) {
    /* if there are no transpositions requested, just make it one of 0.0
     * (i.e. none)   */
    num = 1;
    list = noTransp;
  }
  g->numTranspositions = num;
  for (i = 0; i < num && i < MAXTRANSPOSITIONS; ++i) {
    st = *list++;
    g->transpositions[i] = st;
    g->srcs[i] = st2src(st, g->octaveSize, g->octaveDivisions);
  }
} 

/*****************************************************************************/

void mdeGranularOn(mdeGranular* g)
{
  if (g->status != STARTING && g->status != ON) {
    mdeGranularClearTheSamples(g);
    g->status = STARTING;
  }
}

/*****************************************************************************/

void mdeGranularClearTheSamples(mdeGranular* g)
{
  if (g->live && g->theSamples) {
    silence(g->theSamples, g->nAllocatedBufferSamples);
    g->liveIndex = 0;
  }
}

/*****************************************************************************/

void mdeGranularForceGrainReinit(mdeGranular* g)
{
  int i;
  mdeGranularGrain gg;

  if (g->grains) {
    for (i = 0; i < g->maxVoices; ++i) {
      gg = g->grains[i];
      /* doing this will cause mdeGranularGrainExhaused() to return true
       * so the grains will be reinitialized */ 
      gg.icurrent = gg.length + 1;
    }
  }
}

/*****************************************************************************/

void mdeGranularOff(mdeGranular* g)
{
  if (g->status != STOPPING && g->status != OFF)
    g->status = STOPPING;
}

/*****************************************************************************/

int mdeGranularIsOn(mdeGranular* g)
{
  return (g->status == ON);
}

/*****************************************************************************/

int mdeGranularIsOff(mdeGranular* g)
{
  return (g->status == OFF);
}

/*****************************************************************************/

void mdeGranularGrainPrint(mdeGranularGrain* gg)
{
  post("mdeGranular~ grain info:");
  post("length %ld", gg->length);
  post("start %f", gg->start);
  post("end %f", gg->end);
  post("endRampUp %ld", gg->endRampUp);
  post("startRampDown %ld", gg->startRampDown);
  post("current %f", gg->current);
  post("icurrent %ld", gg->icurrent);
  post("rampi %ld", gg->rampi);
  post("inc %f", gg->inc);
  post("backwards %d", gg->backwards);
  post("status %d", gg->status);
  post("activeStatus %d", gg->activeStatus);
  post("channel %d", gg->channel);
  post("doDelay %d", gg->doDelay);
  post("firstDelay %ld", gg->firstDelay);
  post("firstDelayCounter %ld", gg->firstDelayCounter);
}

/*****************************************************************************/

/** Print the values of the simple variables of the granulator. Arrays are not
 *  printed as of yet.
 *  */

void mdeGranularPrint(mdeGranular* g)
{
  int i;

  post("mdeGranular~ data structure info:");

  for (i = 0; i < g->rampLenSamples; ++i)
    post("i = %d: rampUp = %f, rampDown = %f",
         i, g->rampUp[i], g->rampDown[i]);
  /* print the grain amps array */
  /*
  for (i = 0; i < g->nOutputSamples; ++i)
      post("i = %d: grainAmps = %f",
           i, g->grainAmps[i]);
  */
  post("rampType = %s", g->rampType);
  post("maxVoices %d", g->maxVoices);
  post("activeVoices %d", g->activeVoices);
  post("samplingRate %f", g->samplingRate);
  post("transpositionOffsetST %f", g->transpositionOffsetST);
  post("transpositionOffset %f", g->transpositionOffset);
  post("numTranspositions %d", g->numTranspositions);
  post("grainLengthMS %f", g->grainLengthMS);
  post("grainLength %d", g->grainLength);
  post("grainLengthDeviation %f", g->grainLengthDeviation);
  post("numChannels %d", g->numChannels);
  post("activeChannels %d", g->activeChannels);
  post("nOutputSamples %ld", g->nOutputSamples);
  post("BufferName: %s", g->BufferName);
  post("nBufferSamples %ld", g->nBufferSamples);
  post("BufferSamplesMS %f", g->BufferSamplesMS);
  post("nAllocatedBufferSamples %ld", g->nAllocatedBufferSamples);
  post("AllocatedBufferMS %f", g->AllocatedBufferMS);
  post("samplesStartMS %f", g->samplesStartMS);
  post("samplesStart %ld", g->samplesStart);
  post("samplesEndMS %f", g->samplesEndMS);
  post("samplesEnd %ld", g->samplesEnd);
  post("rampLenMS %f", g->rampLenMS);
  post("rampLenSamples %ld", g->rampLenSamples);
  post("density %f", g->density);
  post("status %d", g->status);
  post("grainAmp %f", g->grainAmp);
  post("lastGrainAmp %f", g->lastGrainAmp);
  post("targetGrainAmp %f", g->targetGrainAmp);
  post("grainAmpInc %f", g->grainAmpInc);
  post("statusRampIndex %ld", g->statusRampIndex);
  post("live %d", g->live);
  post("liveIndex %ld", g->liveIndex);
  post("OctaveSize %f", g->octaveSize);
  post("OctaveDivisions %f", g->octaveDivisions);
  post("PortionPosition %f", g->portionPosition);
  post("PortionWidth %f", g->portionWidth);
  post("============= Grain 1 =============");
  mdeGranularGrainPrint(&g->grains[0]);
}

/*****************************************************************************/

int mdeGranularAtTargetGrainAmp(mdeGranular* g)
{
  if (g)
    return (((g->lastGrainAmp <= g->targetGrainAmp) /* ascending */
             && (g->grainAmp >= g->targetGrainAmp))
            ||
            ((g->lastGrainAmp >= g->targetGrainAmp) /* descending */
             && (g->grainAmp <= g->targetGrainAmp)));
  else return 0;
}

/*****************************************************************************/

mdefloat mdeGranularGetGrainAmpAndInc(mdeGranular* g)
{
  if (g) {
    mdefloat ret = g->grainAmp;

    if (!mdeGranularAtTargetGrainAmp(g))
      g->grainAmp += g->grainAmpInc;
    /* 17/4/08: don't go < 0! */
    if (g->grainAmp < (mdefloat)0.0)
      g->grainAmp = (mdefloat) 0.0;
    return ret;
  }
  else return (mdefloat)0.0;
}

/*****************************************************************************/

/** This function is called at object init and other major points of restart so
 *  not only do we init the grains but we calculate a delay for each grain to
 *  avoid starting/stopping at exactly the same point in each voice.
 *  */

void mdeGranularInitGrains(mdeGranular* g)
{
  int i;

  /* we can't do this until we have the samples! */
  if (g->samples)
    for (i = 0; i < g->maxVoices; ++i)
      mdeGranularGrainInit(&g->grains[i], g, 1);
}

/*****************************************************************************/

void mdeGranularStoreRampType(mdeGranular* g, char* type)
{
  char* new = mdeCalloc(strlen(type) + 1, sizeof(char), 
                        "mdeGranularStoreRampType", g->warnings);
  char* old = g->rampType;

  strcpy(new, type);
  g->rampType = new;
  if (old)
    mdeFree(old);
}

/*****************************************************************************/

/** 3/4/08: the rampLenMS and samplingRate arguments are no longer
 *  used here as they are only valid once the audio has been turned on
 *  and srate selected. Functionality that used these is passed down
 *  to the init2 function.
 */ 
int mdeGranularInit1(mdeGranular* g, int maxVoices, int numChannels)
{
  unsigned seed = (unsigned int)clock();
  /* post("%d %d", (int)maxVoices, (int)numChannels); */

  g->channelBuffers = NULL;
  g->signalIn = NULL;
  g->grains = NULL;
  g->theSamples = NULL;
  g->samples = NULL;
  g->rampUp = NULL;
  g->rampDown = NULL;
  g->grainAmps = NULL;
  g->rampType = NULL;
  g->octaveSize = (mdefloat)2.0;
  g->octaveDivisions = (mdefloat)12.0;
  g->portionPosition = (mdefloat)0.0;
  g->portionWidth = (mdefloat)100.0;

  srand(seed);
  g->warnings = 1;
  g->status = OFF;
  g->statusRampIndex = 0;
  mdeGranularSetMaxVoices(g, maxVoices);
  /* set all the voices active */
  mdeGranularSetActiveVoices(g, maxVoices);
  mdeGranularSetTranspositions(g, 0, NULL);
  g->numChannels = numChannels;
  g->activeChannels = numChannels;
  if (g->channelBuffers)
    mdeFree(g->channelBuffers);
  g->channelBuffers = mdeCalloc(numChannels, sizeof(mdefloat*),
                                "mdeGranularInit1", g->warnings);
  /* call inlet methods */
  mdeGranularSetTranspositionOffsetST(g, (mdefloat)0.0);
  mdeGranularSetGrainLengthDeviation(g, (mdefloat)10.0);
  mdeGranularSetDensity(g, (mdefloat)100.0);
  /* do this so memory is not freed! */
  g->rampType = NULL;
  mdeGranularStoreRampType(g, DEFAULT_RAMP_TYPE);
  return 0;
}

/*****************************************************************************/

/** args are the object, the number of samples to ouput per dsp tick and the
 *  ramp length in millisecs
 *  Called from mdeGranular_tildeDSP i.e. after the audio engine starts.
 * */

int mdeGranularInit2(mdeGranular* g, long nOutputSamples, mdefloat rampLenMS,
                     mdefloat** channelBuffers)
{
  int i;
  if (g) {
    /* 2/4/08: samplingRate has been set in mdeGranular_tildeDSP before this
    function is called;  just make sure though... */
    if (!g->samplingRate)
      error("mdeGranular~: sampling rate has not been set!");
    /* 2/4/08 these two calls used to be done in init1 */
    if (!mdeGranularDidInit(g)) {
      mdeGranularSetGrainLengthMS(g, (mdefloat)50.0);
      mdeGranularSetRampLenMS(g, rampLenMS);
    }
    g->nOutputSamples = nOutputSamples;
    /* Copy the memory addresses of the output channels into the object. PD does
    it this way, Max used to do it this way but now it happens in the perform
    routine */
    if (channelBuffers)
      for (i = 0; i < g->numChannels; ++i)
        g->channelBuffers[i] =  channelBuffers[i];
    /* can't call the inlet method here, have to set it directly */
    if (!mdeGranularDidInit(g)) {
      g->grainAmp = 0.5;
      g->targetGrainAmp = g->grainAmp;
      g->lastGrainAmp = g->grainAmp;
    }
    if (g->grainAmps)
      mdeFree(g->grainAmps);
    g->grainAmps = mdeCalloc(g->nOutputSamples, sizeof(mdefloat),
                             "mdeGranularInit2", g->warnings);
    if (!g->grainAmps)
      error("mdeGranular~: can't allocate memory for the grain amplitudes!");
  }
  return 0;
}

/*****************************************************************************/

int mdeGranularDidInit(mdeGranular* g)
{
  return g->grainAmps ? 1 : 0;
}

/*****************************************************************************/

void mdeGranular_tildeSetF(t_mdeGranular_tilde *x, double bufsize)
{
  mdefloat srate = (mdefloat)sys_getsr();
  mdeGranular* g = &x->x_g;

  if ((bufsize < MINLIVEBUFSIZE)) {
    if (g->warnings)
      post("mdeGranular~: Minimum buffer size is %f millisecs, ignoring %f", 
           MINLIVEBUFSIZE, bufsize);
    return;
  }
  if (mdeGranularInit3(g, NULL, bufsize, ms2samples(srate, bufsize))
      < 0)
    post("mdeGranular~: couldn't init Granular object \n\
                        for live granulation");
}

/*****************************************************************************/

/** Called when a set message is sent to the object
 *  samplesMS is the length of the buffer (-samples-) in millisecs,
 *  numSamples the length of the same in samples.
 *  */

int mdeGranularInit3(mdeGranular* g, mdefloat* samples, mdefloat samplesMS,
                     mdefloat numSamples)
{
  /* post("mdeGranularInit3"); */
  /* we were given the name of a buffer to granulate */
  if (samples) {
    g->samples = samples;
    g->live = 0;
  }
  else { /* live input */
    /* this should only happen at the init stage... */
    if (!g->theSamples)
      /* don't just set max size to samplesMS, rather at init set to 10secs */
      /* mdeGranularSetLiveBufferSize(g, samplesMS); */
      mdeGranularSetLiveBufferSize(g, (mdefloat)10000);
    g->samples = g->theSamples;
    if (numSamples > g->nAllocatedBufferSamples) {
      if (g->warnings) {
        post("mdeGranular~:");
        post("              The allocated live sample buffer is only ");
        post("              %fms so your request for %fms is invalid.",
             g->AllocatedBufferMS, samplesMS);
        post("              Please send the object a \"MaxLiveBufferMS\" ");
        post("              message to increase this (preferably do this at");
        post("              the beginning of your performance, allocating ");
        post("              enough for all the performance's needs).");
      }
      return 1;
    }
    g->live = 1;
    g->liveIndex = 0;
  }
  g->nBufferSamples = (long)numSamples;
  g->BufferSamplesMS = samplesMS;
  /* the DBL_MIN triggers setting the end to the end of the sample buffer */
  mdeGranularSetSamplesEndMS(g, (mdefloat)DBL_MIN);
  /* the DBL_MIN triggers setting the start to the beginning of the sample
   * buffer */
  mdeGranularSetSamplesStartMS(g, (mdefloat)DBL_MIN);
  if (g->nBufferSamples < g->grainLength) {
    int ninetypc = (int)((mdefloat)g->nBufferSamples * (mdefloat)0.9);

    mdefloat ninetypcf = samples2ms(g->samplingRate, ninetypc);
    if (g->warnings) {
      post("mdeGranular~:");
      post("              length of buffer (%fms) to granulate is",
           g->BufferSamplesMS);
      post("              less than the grain length (%fms). Changing grain",
           g->grainLengthMS);
      post("              length to 90 per cent of buffer size (%fms).", 
           ninetypcf);
    }
    g->grainLength = ninetypc;
    g->grainLengthMS = ninetypcf;
  }
  mdeGranularInitGrains(g);
  return 0;
}

/*****************************************************************************/

void mdeGranularFree(mdeGranular* g)
{
#if 1
  if (g->grains) {
    mdeFree(g->grains);
    g->grains = NULL;
  }
  /* ramp down is just a pointer to the middle of rampUp so no need to free
     it */
  if (g->rampUp) {
    mdeFree(g->rampUp);
    g->rampUp = NULL;
  }
  if (g->rampType) {
    mdeFree(g->rampType);
    g->rampType = NULL;
  }
  if (g->channelBuffers) {
    mdeFree(g->channelBuffers);
    g->channelBuffers = NULL;
  }
  if (g->grainAmps) {
    mdeFree(g->grainAmps);
    g->grainAmps = NULL;
  }
  if (g->theSamples) {
    mdeFree(g->theSamples);
    g->theSamples = NULL;
  }
#endif
}

/***************************************************************************/

int mdeGranularGrainInit(mdeGranularGrain* gg, mdeGranular* parent, 
                         int doFirstDelay)
{
  int plen = parent->grainLength;
  long givenStart = parent->samplesStart;
  long givenEnd = parent->samplesEnd;
  /* the grain's sample increment is a randomly chosen transposition from the
   * parent multiplied by the offset from the parent  */
  mdefloat inc =
    parent->srcs[(int)between((mdefloat)0.0,
                              (mdefloat)parent->numTranspositions)] *
    parent->transpositionOffset;
  int ramplength = parent->rampLenSamples;
  int length;
  mdefloat samplesNeeded;
  mdefloat max_start = (mdefloat)0.0;
  mdefloat st;
  mdefloat nd;
  int backwards = givenStart > givenEnd;
  /* whether the grain will produce audio output or not */
  t_status status = ON;
  int ramplength2 = ramplength * 2;
  mdefloat min_start = 0.0;
  int live = parent->live;
  long latestSample = parent->liveIndex;
  long newLiveSamples;
  /* mdefloat fstart;*/

  /* now we know whether we going backwards or forwards, proceed as if we were
   * going forwards anyway and change after we've calculated our data */ 
  if (backwards) {
    givenStart = givenEnd; 
    givenEnd = parent->samplesStart;
  }
  /* fstart = (mdefloat)givenStart; */
  if (gg->activeStatus == INACTIVE) { 
    /* we can switch this grain off now as it's come to the end of its ramp
     * down and it's been turned off */ 
    gg->status = OFF;
    return 1;
  }
  /* if the requested grain length is too low to get the ramps in, change it
   *  accordingly. */
  if (plen < ramplength2)
    plen = ramplength2;
  length = (int)randomlyDeviate((mdefloat)plen, parent->grainLengthDeviation);
  /* Get the number of live samples that will have been written by the time
   * this grain comes to an end. So bear in mind that if we're live, our sample
   * buffer will need to be > twice the grain length */
  newLiveSamples = 
    live ? parent->nOutputSamples * (1 + (length / parent->nOutputSamples))
    : 0;
  samplesNeeded = (mdefloat)length * inc;
  /* without this check we get slight crackling when the grain length
   * approaches ramplength2 */
  if (length < ramplength2)
    status = SKIPGRAIN;
  /* These start/end points are using the whole buffer between user-given start
   * and end. If we're live, user-given start and end have no meaning as the
   * live sample writing will go to the end of the buffer and start again at
   * the beginning: nothing else makes sense. But assume that the user knows
   * what they're doing and ignore this fact (they shouldn't change start and
   * end after setting a live buffer size...but if they want to....).
   *
   * When calculating start and end points, take into consideration how many
   * new samples will have been copied into the buffer by the time we come to
   * the end of the grain; this will ensure that we only play back grains that
   * can finish before the live sample write pointer has caught up; this still
   * works on a 'whole buffer' principle though, at this point; then, when
   * we've done our calculations to get the start and end points, increment
   * them both by the present write pointer to make sure that we don't read
   * over the write pointer; this will mean of course that the end point is
   * potentially off the end of the buffer but the circular buffer reading
   * mechanism in the mdeGranularGrainMixIn and interpolate functions will take
   * care of this.
   */
  else {
    /* newLiveSamples = 0 if we're not live so that's fine */
    min_start = (mdefloat)(givenStart + newLiveSamples);
    max_start = (mdefloat)givenEnd - samplesNeeded;
    if (max_start < min_start)
      /* we don't have enough samples to do this transposition for the
       *  requested grain length */
      status = SKIPGRAIN;
  }
  if (status) {
    /* given the above if/else, start should always be < max_start, right? */
    st = between(min_start, max_start);
    /* if we're not transposing, no point interpolating all the time is there?
     * */ 
    if (inc == 1.0)
      st = (mdefloat)((long)st);
    /* could be < 0 or > buffer size but we wrap later */
    nd = st + samplesNeeded;
  }
  else {
    /* there will be no audio output for this grain but set it up to be 
     * exhausted after the requested grain length anyway, so we can re-init and
     * see if it will work at that point.
     * it doesn't matter if the start or end point now go over sample buffer
     * boundaries as there will be no sample lookup anyway, just simple
     * increment. */
    st = (mdefloat)givenStart;
    nd = (mdefloat)(givenStart + plen);
    inc = 1.0;
  }

  /* this may well push us off the end of the buffer in terms of numbers at
   * least, but this is moduloed back into bounds during interpolation
   * 1.8.10: only add latestSample if we're live, no?
  */
  if (parent->live) {
    st += latestSample;
    nd += latestSample;
  }
  gg->length = length;
  /* post("length=%d", length); */
  gg->start = backwards ? nd : st; 
  gg->end = backwards ? st : nd;
  gg->inc = backwards ? -inc : inc;
  gg->backwards = backwards ? 1 : 0;
  gg->current = gg->start;
  gg->status = status;
  gg->rampi = 0;
  gg->icurrent = 0;
  /* ramp start/stop points take inc into consideration, so the negative or
   * positive gg->inc is important here! 
*/
  gg->endRampUp = ramplength;
  gg->startRampDown = length - ramplength;
  /* channel is selected randomly */
  gg->channel = (int)between((mdefloat)0.0, (mdefloat)parent->activeChannels);
  /* post("gg->channel = %d", gg->channel); */
  /* do density: we can assume that it is >= 0 and <= 100 because of the set
   * method that checks this. */
  if (between((mdefloat)0.0, (mdefloat)100.0) > parent->density)
    gg->status = SKIPGRAIN;
  /* if requested, set a delay of the given number of samples or up to 200% the
   * grain length for this grain */ 
  if (doFirstDelay || gg->doDelay) {
    /* firstDelay is in samples not ms */
    /* at init gg->doDelay has been initialized to 1 by the call to
     * setmaxvoices (which calls setactivevoices, which sets doDelay
     * to 1) in init1 */ 
#ifdef DEBUG
    if (gg->doDelay > 1)
      post("gg->doDelay=%d length=%d", gg->doDelay, gg->length);
#endif 
    /* i.e. doDelay could be the number of samples we already know we want to
     * delay for so use that, otherwise pick a random number */
    gg->firstDelay = (gg->doDelay > 1) ? gg->doDelay :
      (long)between((mdefloat)0.0, gg->length * (mdefloat)2.0);
    gg->firstDelayCounter = 0;
    /* don't do it next time! */
    gg->doDelay = 0;
  }

#ifdef DEBUG
  if (DebugFP)
    fprintf(DebugFP,
            "start %f (bufstart %ld), end %f (bufend %ld), inc %f, "
            "current %f, up %d, down %d, "
            "length %d latestSample %d ",
            gg->start, givenStart, gg->end, givenEnd, gg->inc, gg->current,
            gg->endRampUp, gg->startRampDown, gg->length, latestSample);
#endif

  return 0;
}

/*****************************************************************************/

/** The side-effect here is that status changes when it is detected that ramp
 *  up/down is over 
 *  10.9.10 NB that the ramp used for starting and stopping is exactly the same
 *  (and therefore the same length) as that used for the grains. 
 * */

mdefloat mdeGranularGetAmpForStatus(mdeGranular* g)
{
  double result = 0.0;

  switch (g->status) {
  case STARTING:
    result = g->rampUp ? g->rampUp[g->statusRampIndex++] : 0.0;
    /* this means of course that our fade up (and down--see case
     * STOPPING) is the same length as the grain ramp length
     */ 
    if (g->statusRampIndex >= g->rampLenSamples) {
      g->status = ON;
      g->statusRampIndex = 0;
      result = 1.0;
    }
    break;
  case STOPPING:
    result = g->rampDown[g->statusRampIndex++];
    if (g->statusRampIndex >= g->rampLenSamples) {
      g->status = OFF;
      g->statusRampIndex = 0;
      result = 0.0;
      mdeGranularForceGrainReinit(g);
    }
    break;
    /* probably won't need these last two but for the sake of
     * completeness... */ 
  case ON:
    result = 1.0;
    break;
  case OFF:
    result = 0.0;
    break;
  case SKIPGRAIN:
    break;
  case INACTIVE:
    break;
  case ACTIVE:
    break;
  }
  return (mdefloat)result;
}

/*****************************************************************************/

void mdeGranularGo(mdeGranular* g)
{
  int i;
  int j;
  mdeGranularGrain* gg;
  mdefloat statusRampVal;
  mdefloat* samp;
  long tickSize = g->nOutputSamples;
  mdefloat* gamp = g->grainAmps;

#ifdef DEBUG
  if (gamp)
    fprintf(DebugFP, "gamp=%ld *gamp=%f", gamp, *gamp);
  else fprintf(DebugFP, "gamp=NULL!");
#endif

#ifdef DEBUG
  if (!gamp || !g)
    error("mdeGranular~: gamp is NULL!");
  post("gamp %ld g %ld", gamp, g);
#endif

  /* if we're at the target amp and the first number in our array is the same
   * as the target amp, then we're at steady state and don't need to get the
   * ramp values (however, first time at target amp is not enough: we need to
   * fill the buffer with repeated target amps
   * */
  /* post("gamp %f g %ld", *gamp, g); */
  if (mdeGranularDidInit(g)) {
    if (!(mdeGranularAtTargetGrainAmp(g) && *gamp == g->grainAmp)) {
      for (i = 0; i < tickSize; ++i, ++gamp) {
        *gamp = mdeGranularGetGrainAmpAndInc(g);
      }
    }
  }

  /* zero out the buffers first */
  for (i = 0; i < g->numChannels; ++i)
    silence(g->channelBuffers[i], tickSize);
  if (g->status && g->grains) {
    for (i = 0; i < g->maxVoices; ++i) {
      gg = &g->grains[i];
      mdeGranularGrainMixIn(gg, g, g->channelBuffers[gg->channel],
                            tickSize);
    }
    if (g->status == STARTING || g->status == STOPPING) {
      for (i = 0; i < tickSize; ++i) {
        statusRampVal = mdeGranularGetAmpForStatus(g);
        for (j = 0; j < g->activeChannels; ++j) {
          samp = g->channelBuffers[j] + i;
          *samp *= statusRampVal;
        }
      }
    }
  }
}

/*****************************************************************************/

/** Get -howMany- samples from -samples- and mix them into -where- i.e. mix
 *  with what's already there.
 *  
 */

void mdeGranularGrainMixIn(mdeGranularGrain* gg, mdeGranular* parent, 
                           mdefloat* where, int howMany)
{
  mdefloat rampval;
  mdefloat samp;
  mdefloat out;
  mdefloat* samples = parent->samples;
  mdefloat inc = gg->inc;
  int i;
  
#if 0
  if (gg == NULL)
    post("gg is NULL!");
  if (parent == NULL)
    post("parent is NULL!");
#endif

#ifdef DEBUG
  static int file_count = 1;
  char filename[128];
#endif

  /* only do it if there are samples to granulate and a buffer to write into */
  if (samples && where)
    for (i = 0; i < howMany; ++i) {
      /* are we in the initial delay part for this grain? */
      if (gg->firstDelayCounter < gg->firstDelay)
        gg->firstDelayCounter++;
      else {
        if (mdeGranularGrainExhausted(gg)) {

#ifdef DEBUG
          if (DebugFP) {
            fprintf(DebugFP, "\n end grain");
            fflush(DebugFP);
            fclose(DebugFP);
          }
          sprintf(filename, "/temp/mdeGranular%03d.txt", file_count++);
          DebugFP = fopen(filename, "w");
          if (!DebugFP)
            error("Can't open temp file.");
          fprintf(DebugFP, "%f\n", inc);
#endif
          mdeGranularGrainInit(gg, parent, 0);
          /* don't start back at the beginning--carry on from where we left
           * off, i.e. plus i!!!!!  */
          where = parent->channelBuffers[gg->channel] + i;
          /* 4/8/04: gg->inc will probably have changed!  Not updating
           * the inc local variable to reflect this was almost
           * certainly causing buffer overruns and perhaps even
           * crashes!  */
          inc = gg->inc;
        }
        if (!(gg->status == OFF || gg->status == SKIPGRAIN)) {
          if (inc == (mdefloat)1.0) {
            /* MDE Wed Sep 18 19:47:49 2013 -- we're all 64bit float since Max
             * 6 but buffer~s are still 32bit (damn!) so we'll have to fudge
             * things a little here 
            tmp = (long)gg->current % parent->nBufferSamples;
            samp = parent->live ? *(samples + tmp) : *(fsamples + tmp);
            */
            samp = *(samples + ((long)gg->current % parent->nBufferSamples));
          }
          else samp = interpolate(gg->current, samples, parent->nBufferSamples,
                                  gg->backwards); /*, parent->live);*/
          rampval = mdeGranularGrainGetRampVal(gg, parent->rampUp,
                                               parent->rampDown, 
                                               parent->rampLenSamples);
          out = samp * rampval * parent->grainAmps[i];
          *where += out;    /* mix with what's already there */
#ifdef DEBUG
          if (rampval < (mdefloat)0.0 || rampval > (mdefloat)1.0)
            post("rampval %f", rampval);
          if (*where < (mdefloat)-1.0 || *where > (mdefloat)1.0)
            post("%f", *where);
          if (DebugFP)
            fprintf(DebugFP, "current %f icurrent %d sample %f ramp %f out %f "
                    "where %f nsamples %d this %d\n",
                    gg->current, gg->icurrent, samp, rampval, out, *where,
                    parent->nOutputSamples, gg);
#endif
        }
        /* let these go over the buffer size and modulo later to get the
         * correct sample */
        gg->current += inc;
        /*
        if (gg->current < 0)
            post("gg->current=%f!", gg->current);
        */
        gg->icurrent++;
      }
      ++where;
    }
}

/*****************************************************************************/

/** When we're granulating a live input rather than a static buffer use this
 *  function to copy -nsamps- samples into our samples buffer, incrementing the
 *  index as we go.
 *
 *  NB this always leaves g->LiveIndex at the place for the __next__ incoming
 *  sample, not pointing to the last stored. 
 *  */

void mdeGranularCopyInputSamples(mdeGranular* g, mdefloat* in,
                                 long nsamps) 
{
  mdefloat* samples = g->theSamples;
  long i;
  long li = g->liveIndex;
  long end = g->nBufferSamples;
  
  if (samples) {
    samples += li;
    for (i = 0; i < nsamps; ++i) {
      *samples++ = *in++;
      if (++li == end) {
        li = 0;
        samples = g->theSamples;
      }
    }
    g->liveIndex = li;
  }
}

/*****************************************************************************/

/* MDE Thu Sep 19 09:24:13 2013 -- now that msp is 64 bit, we're still stuck
 * with 32 bit float buffer~s so we'll need to copy samples over and promote to
 * doubles. */

long mdeGranularCopyFloatSamples(mdeGranular* g, float* in, long nsamps) 
{
  mdefloat* samples = g->theSamples;
  long i;
  long num = nsamps;

  if (nsamps > g->nAllocatedBufferSamples && g->warnings) {
    post("mdeGranular~:");
    post("              The allocated live sample buffer is only ");
    post("              %fms but your buffer~ length is %fms ",
         g->AllocatedBufferMS, (mdefloat)nsamps / (g->samplingRate * .001));
    post("              (assuming the buffer~'s sampling rate is the ");
    post("              same as the dac~'s).");
    post("              Please send the object a \"MaxLiveBufferMS\" ");
    post("              message to increase this (preferably do this at");
    post("              the beginning of your performance, allocating ");
    post("              enough for all the performance's needs).");
    num = g->nAllocatedBufferSamples;
  }
  if (samples && in) {
    for (i = 0; i < num; ++i) {
      *samples++ = (mdefloat)*in++;
    }
  }
  return num;
}

/*****************************************************************************/

/**  Get the amplitude scaler for the grain depending on whether we're in the
 *  ramp up, steady state, or ramp down.
 *  */

mdefloat mdeGranularGrainGetRampVal(mdeGranularGrain* gg, 
                                           mdefloat* rampUp, mdefloat* rampDown,
                                           /* rampLen is in samples */
                                           long rampLen)
{
  if (rampUp == NULL || rampDown == NULL)
    return (mdefloat)0.0;
  if (gg->icurrent < gg->endRampUp)
    return rampUp[gg->icurrent];
  else if (gg->icurrent >= gg->startRampDown) {
    /* 10.9.10: to be honest we shouldn't have to do this check as we should
     * never go over the ramp length but in certain circumstances, when
     * changing ramp length we're getting crashes in accessing the ramp down,
     * so just make sure */
    if (gg->rampi < rampLen)
      return rampDown[gg->rampi++];
    else return (mdefloat)0.0;
  }
  else
    return (mdefloat)1.0;
}

/*****************************************************************************/

/** Do we need to reinitialise our grain, i.e. have we finished with the ramp
 *  down?  
 */

int mdeGranularGrainExhausted(mdeGranularGrain* gg)
{
  return (gg->icurrent > gg->length);
}

/*****************************************************************************/
/* 3.10.11 set buffer, grain and ramp lengths simultaneously, thus avoiding all
 * of the length conflicts NB buffer is a symbol, not just a size e.g. ms1000*/ 
void mdeGranularBufferGrainRamp(t_mdeGranular_tilde* x, t_symbol* s,
                                double grain_len, double ramp_len)
{
  mdeGranular* g = &x->x_g;

  if (g->status != OFF) {
    if (g->warnings) {
      post("mdeGranular~:");
      post("              BufferGrainRamp can only be called when off. ");
    }
    return;
  }
  g->grainLength = 0L;
  g->grainLengthMS = (mdefloat)0.0;
  g->rampLenSamples = 0L;
  g->rampLenMS = (mdefloat)0.0;
  mdeGranular_tildeSet(x, s);
  mdeGranularSetGrainLengthMS(g, (mdefloat)grain_len);
  mdeGranularSetRampLenMS(g, (mdefloat)ramp_len);
}

/*****************************************************************************/

void mdeGranularWelcome(void)
{
  post("____________________________________________________");
  post("mdeGranular~");
  post("multi-channel, multi-voice, multi-transposition ");
  post("granular synthesis external for Max/MSP and PD");
  post("Version %s (%s %s)", VERSION, __TIME__, __DATE__);
  post("Michael Edwards ~ m@michael-edwards.org");
  post("____________________________________________________");
}

/*****************************************************************************/



/******************************************************************************
 *                    *********************************************************
 *  HELPER FUNCTIONS  *********************************************************
 *                    *********************************************************
 *****************************************************************************/



/*****************************************************************************/

/** Zero out a bunch of samples (starting at -where-), i.e. make them silent.
 *  */

void silence(mdefloat* where, int numSamples)
{
  if (where)
    memset(where, 0, numSamples * sizeof(mdefloat));
}

/*****************************************************************************/

/** Randomly deviate -number- by maxDeviation; this can be > or < than
 *  -number-; maxDeviation is a percentage 
 *  */

mdefloat randomlyDeviate(mdefloat number, mdefloat maxDeviation)
{
  mdefloat dev = between((mdefloat)0.0, maxDeviation);
  mdefloat ndev = number * (dev * (mdefloat)0.01);

  if (flip())
    ndev = -ndev;
  return number + ndev;
}

/*****************************************************************************/

/** The original straight ramp, called now from makeWindow.
 *  Given the length of a ramp, create the ramp up and ramp down values between
 *  0 and 1. Space for the ramps has already been allocated.
 *  */

void makeRamps(int rampLen, mdefloat* rampUp, mdefloat* rampDown)
{
  mdefloat inc = (mdefloat)1.0 / (mdefloat)(rampLen - 1);
  int i;
  mdefloat f = (mdefloat)0.0;

  for (i = 0; i < rampLen; ++i, f += inc, ++rampUp, ++rampDown) {
    *rampUp = f;
    *rampDown = 1.0f - f;
  }
}

/*****************************************************************************/

/** Convert semitones to sampling-rate conversion factor 
 *  e.g. st2src(-12) -> 0.5,  st2src(12) -> 2,  st2src(0) -> 1
 *  */

mdefloat st2src(mdefloat st, mdefloat octaveSize, 
                       mdefloat octaveDivisions)
{
  return (mdefloat)pow(octaveSize, st / octaveDivisions);
}

/*****************************************************************************/

/** Convert milliseconds to samples using the given sampling rate.
 *  */
long ms2samples(mdefloat samplingRate, mdefloat milliseconds)
{
  return (long)ceil((milliseconds * (mdefloat)0.001) * samplingRate);
}

/*****************************************************************************/

/** Convert samples to milliseconds using the given sampling rate.
 *  */
mdefloat samples2ms(mdefloat samplingRate, int samples)
{
  return (mdefloat)1000.0 * ((mdefloat)samples / samplingRate);
}

/*****************************************************************************/

/** 4-point interpolating table lookup nicked and modified from pd's d_array.c
    (tabread4~)  */

/* MDE Wed Sep 18 20:02:35 2013 -- added live arg: if this is 1 we have 32bit
   float samples, otherwise they're 64bit */
/* MDE Thu Feb 20 11:39:46 2020 -- 'live' arg doesn't seem to be used at all, so
   removing  */
mdefloat interpolate(mdefloat findex, mdefloat* samples, long numSamples,
                     char backwards) /*, char live)*/
{
  long indexTrunc = (long)findex;
  mdefloat fraction =  fabs(findex - (mdefloat)indexTrunc);
  mdefloat a;
  mdefloat b;
  mdefloat c;
  mdefloat d;
  mdefloat cminusb;
  mdefloat* fp;
  mdefloat* lastsamp;
  mdefloat lastsampval;
  mdefloat result;

  if (!samples)
    return (mdefloat)0.0;
  /* do boundary sanity for circular buffer case */
  indexTrunc %= numSamples;
  /* could be the case if going backwards (% doesn't wrap as lisp's mod does so
   * negative numbers remain negative)
   */
  if (indexTrunc < 0) {
    indexTrunc = numSamples + indexTrunc;
  }
  lastsamp = (mdefloat*)(samples + numSamples - 1);
  lastsampval = *lastsamp;
  /* some of these saw samples cast to long but since going 64 bit that no
     longer works (too small?) */ 
  fp = (mdefloat*)(samples + indexTrunc);
  b = *fp;
  if (backwards) {
    a = *(samples + ((indexTrunc + 1) % numSamples));
    c = indexTrunc ? *(fp - 1) : lastsampval;
    d = !indexTrunc ? *(lastsamp - 1) : (indexTrunc == 1 ?
                                         lastsampval : *(fp - 2));
  }
  else {
    a = indexTrunc ? *(fp - 1) : lastsampval;
    c = *(samples + ((indexTrunc + 1) % numSamples));
    d = *(samples + ((indexTrunc + 2) % numSamples));
  }

  cminusb = c - b;
  result = (b + fraction * (cminusb - (mdefloat)0.5 * (fraction - (mdefloat)1.0)
                            * ((a - d + (mdefloat)3.0 * cminusb) * fraction + 
                               (b - a - cminusb))));
#ifdef DEBUG
  if (result > 1.0)
    post("%f at index %d (numSamples: %d, a,b,c,d=%f %f %f %f)\n", 
         result, indexTrunc, numSamples, a, b, c, d);
#endif

  return result;
}

/*****************************************************************************/

/** Return a random number between min (inclusive) and max (exclusive).
 *  */

mdefloat between(mdefloat min, mdefloat max)
{
  /* 2/4/08 the code used to be (mdefloat)(RAND_MAX + 1) but
   * with MacIntel this obviously caused wraparound to a negative
   * int and a negative number to be returned when min and
   * max were both positive!  Should have seen that one coming....
   */
  static const mdefloat div = (mdefloat)RAND_MAX + (mdefloat)1.0;
  
  if (min == max)
    return min;
  else {
    int r = rand();
    mdefloat diff = max - min;
    mdefloat scaler = diff / div;
    return (min + (r * scaler));
  }
}

/*****************************************************************************/

/** Flip of a coin, i.e. return randomly 0 or 1
 *  */

int flip(void)
{
  static const long thresh = (RAND_MAX / 2);
  return (rand() > thresh);
}

/*****************************************************************************/

/** Simple memory allocation for an array using stdlib's calloc (PD) or MaxMSP
 *  API's sysmem_newptrclear but checking for out of memory problem. By using
 *  calloc (PD) or the MAX function, the bytes are guaranteed to be initialized
 *  to zero. */

void* mdeCalloc(int howmany, size_t size, char* caller, char warn)
{
  void* ret = 0x0;

  if (howmany < 1 || size < 1) {
    if (warn)
      post("mdeGranular~: request for 0 bytes (from %s)????", caller);
    return NULL;
  }
#ifdef MAXMSP
  ret = sysmem_newptrclear(howmany * size);
#else
  ret = calloc(howmany, size);
#endif
  if (!ret && warn)
    post("mdeGranular~: mdeCalloc: Out of memory (from %s)!", caller);
  return ret;
}

/*****************************************************************************/

void mdeFree(void* what)
{
#ifdef MAXMSP
  sysmem_freeptr(what);
#else
  free(what);
#endif
}

/*****************************************************************************/

/** Check whether a string contains a number, i.e. only digits and dots.
 *  */

int isanum(char* input)
{
  int ok = 1;

  do {
    if (!isdigit(*input) && *input != '.' && *input != '\0') {
      ok = 0;
      break;
    }
  } while (*input++);
  return ok;
}

/*****************************************************************************/

/* Inlet methods just call portable object's methods */

void mdeGranular_tildeTranspositionOffsetST(t_mdeGranular_tilde* x, mdefloat f)
{
  mdeGranularSetTranspositionOffsetST(&x->x_g, (mdefloat)f);
}
void mdeGranular_tildeGrainLengthMS(t_mdeGranular_tilde* x, mdefloat f)
{
  mdeGranularSetGrainLengthMS(&x->x_g, (mdefloat)f);
}
void mdeGranular_tildeGrainLengthDeviation(t_mdeGranular_tilde* x, mdefloat f)
{
  mdeGranularSetGrainLengthDeviation(&x->x_g, (mdefloat)f);
}
void mdeGranular_tildeSamplesStartMS(t_mdeGranular_tilde* x, mdefloat f)
{
  mdeGranularSetSamplesStartMS(&x->x_g, (mdefloat)f);
}
void mdeGranular_tildeSamplesEndMS(t_mdeGranular_tilde* x, mdefloat f)
{
  mdeGranularSetSamplesEndMS(&x->x_g, (mdefloat)f);
}
void mdeGranular_tildeDensity(t_mdeGranular_tilde* x, mdefloat f)
{
  mdeGranularSetDensity(&x->x_g, (mdefloat)f);
}
void mdeGranular_tildeActiveChannels(t_mdeGranular_tilde* x, long l)
{
  mdeGranularSetActiveChannels(&x->x_g, l);
}
void mdeGranular_tildeWarnings(t_mdeGranular_tilde* x, long l)
{
  mdeGranularSetWarnings(&x->x_g, l);
}
void mdeGranular_tildeGrainAmp(t_mdeGranular_tilde* x, mdefloat f)
{
  mdeGranularSetGrainAmp(&x->x_g, (mdefloat)f);
}
void mdeGranular_tildeMaxVoices(t_mdeGranular_tilde* x, mdefloat f)
{
  mdeGranularSetMaxVoices(&x->x_g, (mdefloat)f);
}
void mdeGranular_tildeActiveVoices(t_mdeGranular_tilde* x, mdefloat f)
{
  mdeGranularSetActiveVoices(&x->x_g, (mdefloat)f);
}
void mdeGranular_tildeRampLenMS(t_mdeGranular_tilde* x, mdefloat f)
{
  mdeGranularSetRampLenMS(&x->x_g, (mdefloat)f);
}
void mdeGranular_tildeRampType(t_mdeGranular_tilde *x, t_symbol *s)
{
  mdeGranularSetRampType(&x->x_g, (char*)s->s_name);
}
void mdeGranular_tildeOn(t_mdeGranular_tilde *x)
{
  mdeGranularOn(&x->x_g);
}
void mdeGranular_tildeOff(t_mdeGranular_tilde *x)
{
  mdeGranularOff(&x->x_g);
}
void mdeGranular_tildeSetLiveBufferSize(t_mdeGranular_tilde *x, mdefloat f)
{
  mdeGranularSetLiveBufferSize(&x->x_g, (mdefloat)f);
}
void mdeGranular_tildeDoGrainDelays(t_mdeGranular_tilde *x)
{
  mdeGranularDoGrainDelays(&x->x_g);
}
void mdeGranular_tildeSmoothMode(t_mdeGranular_tilde *x)
{
  mdeGranularSmoothMode(&x->x_g);
}
void mdeGranular_tildeOctaveSize(t_mdeGranular_tilde *x, mdefloat f)
{
  mdeGranularOctaveSize(&x->x_g, (mdefloat)f);
}
void mdeGranular_tildeOctaveDivisions(t_mdeGranular_tilde *x, mdefloat f)
{
  mdeGranularOctaveDivisions(&x->x_g, (mdefloat)f);
}
void mdeGranular_tildePortion(t_mdeGranular_tilde *x, mdefloat position, 
                              mdefloat width)
{
  mdeGranularPortion(&x->x_g, (mdefloat)position, (mdefloat)width);
}
void mdeGranular_tildePortionPosition(t_mdeGranular_tilde *x, mdefloat position)
{
  mdeGranularPortionPosition(&x->x_g, (mdefloat)position);
}
void mdeGranular_tildePortionWidth(t_mdeGranular_tilde *x, mdefloat width)
{
  mdeGranularPortionWidth(&x->x_g, (mdefloat)width);
}
void mdeGranular_tildeBufferGrainRamp(t_mdeGranular_tilde *x, t_symbol *s,
                                      mdefloat grain_len, mdefloat ramp_len)
{
  mdeGranularBufferGrainRamp(x, s, grain_len, ramp_len);
}

/*****************************************************************************/


/****************************************************************************
 *************************                    *********************************
 ************************* WINDOWS FOR RAMPS  *********************************
 *************************                    *********************************
 *****************************************************************************/


/*****************************************************************************/

/** This section taken (and modified slightly) from Bill Schottstaedt's CLM
 *  (clm.c) windowing functions (for FFT).
 *  (http://www-ccrma.stanford.edu/software/clm)
 *  */

/*****************************************************************************/

mdefloat square(mdefloat x) 
{
  return(x * x);
}

/*****************************************************************************/

double mus_bessi0(mdefloat x)
{ 
  double z, denominator, numerator;
  if (x == (mdefloat)0.0) 
    return(1.0);
  if (fabs(x) <= 15.0) 
    {
      z = x * x;
      numerator = 
        (z * 
         (z *
          (z *
           (z *
            (z * 
             (z *
              (z *
               (z *
                (z *
                 (z *
                  (z * 
                   (z *
                    (z * 
                     (z * 
                      0.210580722890567e-22 + 0.380715242345326e-19) +
                     0.479440257548300e-16) + 0.435125971262668e-13) +  
                   0.300931127112960e-10) + 0.160224679395361e-7) +
                 0.654858370096785e-5) + 0.202591084143397e-2) +
               0.463076284721000e0) + 0.754337328948189e2) +
             0.830792541809429e4) + 0.571661130563785e6) +
           0.216415572361227e8) + 0.356644482244025e9) +
         0.144048298227235e10);
      denominator = (z * (z * (z - 0.307646912682801e4) +
                          0.347626332405882e7) - 0.144048298227235e10);
      return (-numerator / denominator);
    } 
  return(1.0);
}

/*****************************************************************************/

#ifndef M_PI
#define M_PI (mdefloat)3.14159265358979323846264338327
#endif
#ifndef TWO_PI
#define TWO_PI ((mdefloat)2.0 * M_PI)
#endif

/*****************************************************************************/

/** The Kaiser, Cauchy, Poisson, Gaussian, and Tukey windows all use the beta
 *  argument. What it is I don't know (will find out when I have some time)
 *  but CLM uses the default argument of 2.5 so we'll go with that for now.
 *  */

mdefloat* makeWindow(char* type, int size, mdefloat beta, mdefloat *window)
{
  /** Bill says: 
   *    mostly taken from Fredric J. Harris, "On the Use of Windows for
   *    Harmonic Analysis with the Discrete Fourier Transform," Proceedings of
   *    the IEEE, Vol. 66, No. 1, January 1978. Albert H. Nuttall, "Some
   *    Windows with Very Good Sidelobe Behaviour", IEEE Transactions of
   *    Acoustics, Speech, and Signal Processing, Vol. ASSP-29, No. 1, February
   *    1981, pp 84-91
   *
   * JOS had slightly different numbers for the Blackman-Harris windows. */

  int i, j, midn, midp1;
  mdefloat freq, rate, sr1, angle, expn, expsum, I0beta, cx;

  if (window == NULL) 
    error("No memory for ramp!");

  /* Bill assumes a power of 2 window size, we don't: */
  midn = size / 2; /* size >> 1; */
  midp1 = (size + 1) / 2;
  freq = TWO_PI / (mdefloat)size;
  rate = (mdefloat)1.0 / (mdefloat)midn;
  angle = (mdefloat)0.0;
  expn = (mdefloat)log(2) / (mdefloat)(midn + 1);
  expsum = (mdefloat)1.0;

  /* This one is mine, not Bill's */
  if (!strcmp(type, "TRAPEZOID")) { 
    /** actually an isosceles trapezoid as the two angles are the same */
    makeRamps(midn, window, window + midn);
  }
  else if (!strcmp(type, "RECTANGULAR")) {
    for (i = 0; i < size; i++) 
      window[i] = (mdefloat)1.0;
  }
  /* does go from 0 to 1 and back */
  else if (!strcmp(type, "HANN") || !strcmp(type, "HANNING")) {
    for (i = 0, j = size - 1, angle = (mdefloat)0.0; i <= midn; 
         i++, j--, angle += freq) 
      window[j] = (window[i] = (mdefloat)(0.5 - 0.5 * cos(angle)));
  }
  /* does go from 0 to 1 and back */
  else if (!strcmp(type, "WELCH")) {
    for (i = 0, j = size - 1; i <= midn; i++, j--)
      window[j] = (window[i] = 
                   (mdefloat)1.0 - square((mdefloat)(i - midn) /
                                          (mdefloat)midp1));
  }
  /* does go from 0 to 1 and back */
  else if (!strcmp(type, "PARZEN")) {
    for (i = 0, j = size - 1; i <= midn; i++, j--)
      window[j] = (window[i] = 
                   (mdefloat)(1.0 - fabs((mdefloat)(i - midn) / midp1)));
  }
  /* does go from 0 to 1 and back */
  else if (!strcmp(type, "BARTLETT")) {
    for (i = 0, j = size - 1, angle = (mdefloat)0.0; i <= midn;
         i++, j--, angle += rate)
      window[j] = (window[i] = angle);
  } 
  /* doesn't start/return to 0 rather 0.08! */
  else if (!strcmp(type, "HAMMING")) {
    for (i = 0, j = size - 1, angle = (mdefloat)0.0; i <= midn;
         i++, j--, angle += freq)
      {
      window[j] = (window[i] = (mdefloat)(0.54 - 0.46 * cos(angle)));
      }
  }
  /* ranges from 0049 to 1 */
  else if (!strcmp(type, "BLACKMAN2")) {
    /* using Chebyshev polynomial equivalents here */
    for (i = 0, j = size - 1, angle = (mdefloat)0.0; 
         i <= midn; i++, j--, angle += freq)
      {  /* (+ 0.42323 (* -0.49755 (cos a)) (* 0.07922 (cos (* a 2)))) */
        cx = (mdefloat)cos(angle);
        window[j] = 
          (window[i] = 
           ((mdefloat)0.34401 + 
            (cx * ((mdefloat)-0.49755 + (cx * (mdefloat)0.15844)))));
      }
  } 
  /* ranges from 00006 to 1 */
  else if (!strcmp(type, "BLACKMAN3")) {
    for (i = 0, j = size - 1, angle = 0.0; i <= midn; i++, j--, angle += freq) 
      { /* (+ 0.35875 (* -0.48829 (cos a)) 
           (* 0.14128 (cos (* a 2))) (* -0.01168 (cos (* a 3)))) */
        /* (+ 0.36336 (*  0.48918 (cos a))
           (* 0.13660 (cos (* a 2))) (*  0.01064 (cos (* a 3))))
           is "Nuttall" window? */
        cx = (mdefloat)cos(angle);
        window[j] = 
          (window[i] =
           ((mdefloat)0.21747 + 
            (cx * ((mdefloat)-0.45325 + 
                   (cx * ((mdefloat)0.28256 - (cx * (mdefloat)0.04672)))))));
      }
  }
  /* range from 0.001857 to 0.999997 */
  else if (!strcmp(type, "BLACKMAN4")) {
    for (i = 0, j = size - 1, angle = 0.0; i <= midn;
         i++, j--, angle += freq) 
      {  /* (+ 0.287333 (* -0.44716 (cos a)) 
            (* 0.20844 (cos (* a 2))) (* -0.05190 (cos (* a 3)))
            (* 0.005149 (cos (* a 4)))) */
        cx = (mdefloat)cos(angle);
        window[j] = (window[i] = 
                     ((mdefloat)0.084037 + 
                      (cx * ((mdefloat)-0.29145 + 
                             (cx * ((mdefloat)0.375696 + 
                                    (cx * ((mdefloat)-0.20762 +
                                           (cx * (mdefloat)0.041194)))))))));
      }
  }
  /* broken, don't use!!!! */
  else if (!strcmp(type, "EXPONENTIAL")) {
    for (i = 0, j = size - 1; i <= midn; i++, j--) {
      window[j] = (window[i] = expsum - (mdefloat)1.0); 
      expsum *= expn;
    }
  }
  /* ranges from 0.303966 to 1.000000 */
  else if (!strcmp(type, "KAISER")) {
    I0beta = (mdefloat)mus_bessi0(beta); /* Harris multiplies beta by pi */
    for (i = 0, j = size - 1, angle = (mdefloat)1.0; i <= midn;
         i++, j--, angle -= rate) 
      window[j] = (window[i] = 
                   (mdefloat)(mus_bessi0((mdefloat)
                                         (beta * sqrt(1.0 - 
                                                      (double)square(angle))))
                              / I0beta));
  }
  /* ranges from 0.137931 to 1.000000 */
  else if (!strcmp(type, "CAUCHY")) {
    for (i = 0, j = size - 1, angle = (mdefloat)1.0; i <= midn;
         i++, j--, angle -= rate)
      window[j] = (window[i] = (mdefloat)1.0 /
                   ((mdefloat)1.0 + square(beta * angle)));
  }
  /* ranges from 0.082085 to 1.000000 */
  else if (!strcmp(type, "POISSON")) {
    for (i = 0, j = size - 1, angle = (mdefloat)1.0; i <= midn;
         i++, j--, angle -= rate)
      window[j] = (window[i] = (mdefloat)exp((double)((-beta) * angle)));
  }
  /* does go from 0 to 1 and back */  
  else if (!strcmp(type, "RIEMANN")) {
    sr1 = TWO_PI / (mdefloat)size;
    for (i = 0, j = size - 1; i <= midn; i++, j--) 
      {
        if (i == midn) 
          window[j] = (window[i] = (mdefloat)1.0);
        else 
          {
            cx = sr1 * (midn - i); 
            /* split out because NeXT C compiler can't handle the full
               expression */
            window[i] = (mdefloat)sin(cx) / cx;
            window[j] = window[i];
          }
      }
  }
  /* ranges from 0.043937 to 1.000000 */
  else if (!strcmp(type, "GAUSSIAN")) {
    for (i = 0, j = size - 1, angle = (mdefloat)1.0; i <= midn; 
         i++, j--, angle -= rate)
      window[j] = (window[i] = (mdefloat)exp((mdefloat)-0.5 
                                             * square(beta * angle)));
  }
  /* seems to be broken as it's just a bunch of 1s */
  else if (!strcmp(type, "TUKEY")) {
    cx = midn * ((mdefloat)1.0 - beta);
    for (i = 0, j = size - 1; i <= midn; i++, j--) 
      {
        if (i >= cx) 
          window[j] = (window[i] = (mdefloat)1.0);
        else window[j] = (window[i] = (mdefloat)0.5 * 
                          ((mdefloat)1.0 - 
                           (mdefloat)cos(M_PI * i / cx)));
      }
  }
  else error("unknown ramp type: %s\n", type); 
  return(window);
}

/*****************************************************************************/

/* EOF mdeGranular~.c */
