/******************************************************************************
 *
 * File:             mdeGranular~maxmsp.c
 *
 * Author:           Michael Edwards - m@michael-edwards.org - 
 *                   http://www.michael-edwards.org
 *
 * Date:             June 13th 2003
 *
 * $$ Last modified:  12:09:28 Mon Sep 19 2022 CEST
 *
 * Purpose:          MAX/MSP interface to the external for multi-channel, 
 *                   multi-voice, multi-transposition granular synthesis.
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
 *                   PARTICULAR PURPOSE. See the GNU General Public License
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

#ifdef MAXMSP

/*****************************************************************************/

void* mdeGranular_tildeClass;

/*****************************************************************************/

/** This is called second, after main  */

void* mdeGranular_tildeNew(long maxVoices, long numChannels)
{  
  t_mdeGranular_tilde* x =
    (t_mdeGranular_tilde *)object_alloc(mdeGranular_tildeClass);
  int i;
  mdeGranular* g = &x->x_g;

  if (!maxVoices || !numChannels)
    post("mdeGranular~ warning: this object takes two arguments: number of \
         voices and number of output channels. The defaults are 10 and 2.");
  if (!maxVoices)
    maxVoices = 10;
  if (!numChannels)
    numChannels = 2;

  /* post("%d %d", (int)maxVoices, (int)numChannels);*/

  dsp_setup((t_pxobject*)x, 1);

  /* couple an inlet to a method: */
  /* inlets have to be defined in reverse order! */
  floatin(x, 1); /* grain amplitude */
  floatin(x, 2); /* density of the grains in % */
  floatin(x, 3); /* end point in buffer in millisecs */
  floatin(x, 4); /* start point in buffer in millisecs */
  floatin(x, 5); /* grain length deviation in percentage of the grain length */
  floatin(x, 6); /* grain length in milliseconds */
  floatin(x, 7); /* transposition offset in semitones */ 
  /* x->x_arrayname = buffer; */
  /* this ensures that a 1000ms buffer will be allocated when the DSP
   * method is called */  
  x->x_arrayname = gensym("ms1000");
  x->x_liverunning = 1;
  /* 2/4/08: no longer pass ramp len and srate here as they're now
   * used in init2 once audio is turned on
   */
  mdeGranularInit1(g, (int)maxVoices, (int)numChannels);
  for (i = 0; i < (int)numChannels; i++)
    outlet_new((t_object*)x, "signal");
  /* MDE Thu Sep 19 10:41:07 2013 -- do this here now as srate is always
   * available */  
  g->samplingRate = sys_getsr(); 
  mdeGranularInit2(g, sys_getblksize(), (mdefloat)DEFAULT_RAMP_LEN,
                   (mdefloat**)0x0);
  mdeGranular_tildeSet(x, x->x_arrayname);
  /* post("gl %f", x->x_g.grainLengthMS); */
  return (x);
}

/*****************************************************************************/

/** This is the function called by MAX/MSP when the cursor is over an inlet or
 *  outlet. We can ignore the box arg for now. The message is either 1
 *  (inlet) or 2 (outlet) depending on whether the cursor is over an inlet or
 *  outlet. arg is the inlet/outlet index starting from 0 for the leftmost.
 *  dstString is the placeholder for the message we want to display; this is
 *  limited to 60 chars.
 *  */

void mdeGranular_tildeAssist(t_mdeGranular_tilde *x, void *box, long message,
                             long arg, char *dstString)
{
  if (message == 2)
    sprintf(dstString, "(signal) granulated output");
  else {
    switch (arg) {    
    case 0:
      sprintf(dstString, "(bang/list/message) bang starts granulation...");
      break;
    case 1:
      sprintf(dstString, "(float) Transposition offset in semitones");
      break;
    case 2:
      sprintf(dstString, "(float) Grain length in milliseconds");
      break;
    case 3:
      sprintf(dstString, "(float) Grain length deviation in percentage \
                          of the grain length");
      break;
    case 4:
      sprintf(dstString, "(float) Start point in buffer in millisecs");
      break;
    case 5:
      sprintf(dstString, "(float) End point in buffer in millisecs");
      break;
    case 6:
      sprintf(dstString, "(float) Density of the grains in percent");
      break;
    case 7:
      sprintf(dstString, "(float) Grain amplitude");
      break;
    }
  }
}

/*****************************************************************************/

/** Turns on granulating of the live input. */

void mdeGranular_tildeLivestart(t_mdeGranular_tilde *x)
{
  x->x_liverunning = 1;
}

/*****************************************************************************/

/** Turns off granulating of the live input thus leaving the buffer with what's
 *  already there (sample and hold). */

void mdeGranular_tildeLivestop(t_mdeGranular_tilde *x)
{
  x->x_liverunning = 0;
}

/*****************************************************************************/

/** Print the state of the mdeGranulator object's inner variables. */

void mdeGranular_tildePrint(t_mdeGranular_tilde *x)
{
  mdeGranularPrint(&x->x_g);
  post("x_liverunning = %d", x->x_liverunning);
}

/*****************************************************************************/

/** This gets called when you send the object a set message with the name of
 *  the array to granulate. It is also called from mdeGranular_tildeDSP
 *  (fourth), i.e. when the audio engine starts.
 * */

void mdeGranular_tildeSet(t_mdeGranular_tilde *x, t_symbol *s)
{
  float* samples;
  int nsamples;
  t_symbol *ps_buffer = gensym("buffer~");
  mdefloat srate = (mdefloat)sys_getsr();
  mdeGranular* g = &x->x_g;
  int got_ms = strncmp(s->s_name, "ms", 2) == 0;
  t_buffer_ref* bref = buffer_ref_new((t_object*)x, s);
  t_buffer_obj* bobj = buffer_ref_getobject(bref);
  long copied;

  /* MDE Thu Sep 19 10:39:17 2013 -- in case it's changed, might as well update
   */ 
  g->samplingRate = srate;
  strncpy(g->BufferName, s->s_name, sizeof(g->BufferName));
  /* post("%s", g->BufferName); */
  if ((got_ms && isanum((char*)(s->s_name + 2))) || isanum((char*)s->s_name)) {
    /* we got a millisecond buffer size e.g. "ms1000" for live input */
    mdefloat bufsize = atof((char*)(s->s_name + (got_ms ? 2 : 0)));
    mdeGranular_tildeSetF(x, bufsize);
  }
  else { /* static buffer */
    x->x_arrayname = s;
    if ((bobj = (t_buffer_obj *)(s->s_thing)) && ob_sym(bobj) == ps_buffer) {
      if ((buffer_getchannelcount(bobj) != 1) && g->warnings) {
        post("mdeGranular~: Only mono buffers allowed: %s", s->s_name);
        return;
      }
      nsamples = buffer_getframecount(bobj);
      samples = buffer_locksamples(bobj);
      copied = mdeGranularCopyFloatSamples(g, samples, nsamples);
      mdegranular_tildeUnlockBuffer(bref);
      if (!samples || mdeGranularInit3(g, g->theSamples,
                                       samples2ms(srate, nsamples), copied)
          < 0)
        post("mdeGranular~: couldn't init Granular object");
    }
    else {
      if (g->warnings)
        post("mdeGranular~: %s: no such array", s->s_name);
      samples = NULL;
    }
  }
}

/*****************************************************************************/
void mdegranular_tildeUnlockBuffer(t_buffer_ref* buf)
{
  if (buf) {
    buffer_unlocksamples(buffer_ref_getobject(buf));
    object_free(buf);
  }
}

/*****************************************************************************/

/** This is called every 64 samples or whatever the tick size is.
 *  */

void mspExternalPerform(t_mdeGranular_tilde* x, t_object* dsp64, double** ins,
                        long numins, double** outs, long numouts,
                        long sampleframes, long flags, void* userparam)  

{
  mdefloat* in = (mdefloat*)ins[0];
  mdeGranular* g = &x->x_g;
  int i;

  for (i = 0; i < g->numChannels; ++i) {
    g->channelBuffers[i] = (mdefloat*)outs[i];
    /* post("%ld", g->channelBuffers[i]); */        
  }
  if (g->live && x->x_liverunning && g->status)
    mdeGranularCopyInputSamples(g, in, sampleframes);

#ifdef DEBUG
  if (DebugFP)
    fprintf(DebugFP, "\n\nPerform\n\n");  
#endif

  mdeGranularGo(g);
  /*
    post("toffset %f", x->x_g.transpositionOffsetST);
    post("glen %f", x->x_g.grainLengthMS);
    post("glenDev %f", x->x_g.grainLengthDeviation);
    post("start %f", x->x_g.samplesStartMS);
    post("end %f", x->x_g.samplesEndMS);
    post("ramplen %f", x->x_g.rampLenMS);
    post("density %f", x->x_g.density);
    post("gamp %f", x->x_g.grainAmp);
  */
}

/*****************************************************************************/

/** This gets called third, when the audio engine starts (after _new!). */

void mdeGranular_tildeDSP(t_mdeGranular_tilde* x, t_object* dsp64, short* count,
                          double samplerate, long vectorsize, long flags)
{
  object_method(dsp64, gensym("dsp_add64"), x, mspExternalPerform, 0, NULL);
}

/*****************************************************************************/

/** This gets called when we receive a bang */

void mdeGranular_tildeBang(t_mdeGranular_tilde *x)
{
  mdeGranular* g = &x->x_g;

  if (mdeGranularIsOn(g))
    mdeGranularOff(g);
  else if (mdeGranularIsOff(g))
    mdeGranularOn(g);
}

/*****************************************************************************/

/** this gets called when a list is sent to the object */

void mdeGranular_tildeList(t_mdeGranular_tilde *x, t_symbol *s,
                           short argc, t_atom *argv)
{
  static mdefloat semitones[MAXTRANSPOSITIONS];
  int i;

  UNUSED(s);
  for (i = 0; i < argc && i < MAXTRANSPOSITIONS; ++i)
    semitones[i] = atom_getfloatarg(i, argc, argv);
  mdeGranularSetTranspositions(&x->x_g, argc, semitones);
}

/*****************************************************************************/

void mdeGranular_tildeFree(t_mdeGranular_tilde *x)
{
  mdeGranular* g = &x->x_g;

  mdeGranularFree(g);
  dsp_free((t_pxobject*)x);
}

/*****************************************************************************/

/* This method is called first. */

int C74_EXPORT main(void)
{
  t_class* c = class_new("mdeGranular~", (method)mdeGranular_tildeNew,
                         (method)mdeGranular_tildeFree,
                         (short)sizeof(t_mdeGranular_tilde),
                         /* 0L, A_DEFFLOAT, A_DEFFLOAT, 0); */
                         /* MDE Fri Feb 21 09:03:38 2020 */
                         0L, A_DEFLONG, A_DEFLONG, 0);
  /* to couple an inlet to a method */
  class_addmethod(c, (method)mdeGranular_tildeTranspositionOffsetST, "ft7",
                  A_FLOAT, 0);
  class_addmethod(c, (method)mdeGranular_tildeGrainLengthMS, "ft6", A_FLOAT, 0);
  class_addmethod(c, (method)mdeGranular_tildeGrainLengthDeviation, "ft5",
                  A_FLOAT, 0);
  class_addmethod(c, (method)mdeGranular_tildeSamplesStartMS, "ft4", A_FLOAT,
                  0);
  class_addmethod(c, (method)mdeGranular_tildeSamplesEndMS, "ft3", A_FLOAT, 0);
  class_addmethod(c, (method)mdeGranular_tildeDensity, "ft2", A_FLOAT, 0);
  class_addmethod(c, (method)mdeGranular_tildeGrainAmp, "ft1", A_FLOAT, 0);
  class_addmethod(c, (method)mdeGranular_tildeBang, "bang", 0); /* start/stop */
  class_addmethod(c, (method)mdeGranular_tildeList, "list", 
                  A_GIMME, 0); /* transpositions */
  class_addmethod(c, (method)mdeGranular_tildeLivestart, "livestart", 0);
  class_addmethod(c, (method)mdeGranular_tildeLivestop, "livestop", 0);
  class_addmethod(c, (method)mdeGranular_tildePrint, "print", 0);
  class_addmethod(c, (method)mdeGranular_tildeSet, "set", A_DEFSYM, 0);
  class_addmethod(c, (method)mdeGranular_tildeSetF, "setms", A_DEFFLOAT, 0);
  class_addmethod(c, (method)mdeGranular_tildeRampType, "RampType", A_DEFSYM,
                  0);
  class_addmethod(c, (method)mdeGranular_tildeRampLenMS, "RampLenMS",
                  A_DEFFLOAT, 0);
  class_addmethod(c, (method)mdeGranular_tildeMaxVoices, "MaxVoices",
                  A_DEFFLOAT, 0);
  class_addmethod(c, (method)mdeGranular_tildeActiveVoices, "ActiveVoices", 
                  A_DEFFLOAT, 0);
  class_addmethod(c, (method)mdeGranular_tildeTranspositionOffsetST, 
                  "TranspositionOffsetST", A_DEFFLOAT, 0);
  class_addmethod(c, (method)mdeGranular_tildeGrainLengthMS, "GrainLengthMS", 
                  A_DEFFLOAT, 0);
  class_addmethod(c, (method)mdeGranular_tildeGrainLengthDeviation,
                  "GrainLengthDeviation", A_DEFFLOAT, 0);
  class_addmethod(c, (method)mdeGranular_tildeSamplesStartMS, "SamplesStartMS",
                  A_DEFFLOAT, 0);
  class_addmethod(c, (method)mdeGranular_tildeSamplesEndMS, "SamplesEndMS",
                  A_DEFFLOAT, 0);
  class_addmethod(c, (method)mdeGranular_tildeDensity, "Density", A_DEFFLOAT,
                  0);
  class_addmethod(c, (method)mdeGranular_tildeGrainAmp, "GrainAmp", A_DEFFLOAT,
                  0);
  class_addmethod(c, (method)mdeGranular_tildeActiveChannels, "ActiveChannels", 
                  A_DEFLONG, 0);
  class_addmethod(c, (method)mdeGranular_tildeAssist,"assist", A_CANT,0);
  class_addmethod(c, (method)mdeGranular_tildeDSP, "dsp64", A_CANT, 0);
  class_addmethod(c, (method)mdeGranular_tildeOn, "on", 0);
  class_addmethod(c, (method)mdeGranular_tildeOff, "off", 0);
  class_addmethod(c, (method)mdeGranular_tildeDoGrainDelays, "DoGrainDelays",
                  0);
  class_addmethod(c, (method)mdeGranular_tildeSmoothMode, "SmoothMode", 0);
  class_addmethod(c, (method)mdeGranular_tildeSetLiveBufferSize, 
                  "MaxLiveBufferMS",  A_DEFFLOAT, 0);
  class_addmethod(c, (method)mdeGranular_tildeOctaveSize, "OctaveSize",
                  A_DEFFLOAT, 0);
  class_addmethod(c, (method)mdeGranular_tildeOctaveDivisions,
                  "OctaveDivisions", A_DEFFLOAT, 0);
  class_addmethod(c, (method)mdeGranular_tildeWarnings, "Warnings", A_DEFLONG,
                  0);
  class_addmethod(c, (method)mdeGranular_tildePortion, "Portion", A_DEFFLOAT, 
                  A_DEFFLOAT, 0);
  class_addmethod(c, (method)mdeGranular_tildePortionPosition,
                  "PortionPosition", A_DEFFLOAT, 0);
  class_addmethod(c, (method)mdeGranular_tildePortionWidth, "PortionWidth", 
                  A_DEFFLOAT, 0);
  class_addmethod(c, (method)mdeGranular_tildeBufferGrainRamp,
                  "BufferGrainRamp", A_DEFSYM, A_DEFFLOAT, A_DEFFLOAT, 0);
  class_dspinit(c);
  class_register(CLASS_BOX, c);
  mdeGranular_tildeClass = c;
  mdeGranularWelcome();
  return 0;
}

/*****************************************************************************/

#endif /* MAXMSP */

/*****************************************************************************/

/* EOF mdeGranular~maxmsp.c */
