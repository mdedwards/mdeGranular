/******************************************************************************
 *
 * File:             mdeGranular~pd.c
 *
 * Author:           Michael Edwards - m@michael-edwards.org - 
 *                   http://www.michael-edwards.org
 *
 * Date:             June 13th 2003
 *
 * $$ Last modified:  15:05:26 Tue Mar 10 2020 CET
 *
 * Purpose:          PD interface to the external for multi-channel, 
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

#ifdef PD

#include "m_pd.h"

/*****************************************************************************/

/* PD seems to really need the _ before tilde! */
static t_class *mdeGranular_tildeClass;

/*****************************************************************************/

/** This is called second, after _setup  */

void *mdeGranular_tildeNew(t_float maxVoices, t_float numChannels)
{
  t_mdeGranular_tilde *x =
    (t_mdeGranular_tilde *)pd_new(mdeGranular_tildeClass);
  int i;
  mdeGranular* g = &x->x_g;

  if (!maxVoices || !numChannels)
    post("mdeGranular~ warning: this object takes two arguments: number of \
         \nvoices and number of output channels. The defaults are 10 and 2.");
  if (!maxVoices)
    maxVoices = 10.0;
  if (!numChannels)
    numChannels = 2.0;
  
  g->samplingRate = sys_getsr(); 
  x->x_arrayname = gensym("ms1000");
  x->x_f = 0;
  x->x_liverunning = 1;
  mdeGranularInit1(g, maxVoices, numChannels);
  for (i = 0; i < (int)numChannels; i++)
    outlet_new(&x->x_obj, gensym("signal"));

  /* couple an inlet to a method:
   * class_addmethod must also be called in setup below
   * */
  inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("float"), 
            gensym("TranspositionOffsetST"));
  inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("float"), 
            gensym("GrainLengthMS"));
  inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("float"),
            gensym("GrainLengthDeviation"));
  inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("float"),
            gensym("SamplesStartMS"));
  inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("float"),
            gensym("SamplesEndMS"));
  inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("float"), gensym("Density"));
  inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("float"), gensym("GrainAmp"));
  return (x);
}

/*****************************************************************************/

/** Turns on granulating of the live input. */

void mdeGranular_tildeLivestart(t_mdeGranular_tilde *x)
{
  x->x_liverunning = 1;
}

/*****************************************************************************/

/** Turns off granulating of the live input thus leaving the buffer with what's
 *  already there (sample and hold).
 *  */

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
  t_garray *a;
  mdefloat* samples;
  int nsamples;
  mdefloat srate = (mdefloat)sys_getsr();
  int got_ms = strncmp(s->s_name, "ms", 2) == 0;
  mdeGranular* g = &x->x_g;

  /* MDE Thu Sep 19 10:39:17 2013 -- in case it's changed, might as well update
   */ 
  g->samplingRate = srate;
  strncpy(g->BufferName, s->s_name, sizeof(g->BufferName));

  if ((got_ms && isanum((char*)(s->s_name + 2))) || isanum((char*)s->s_name)) {
    /* we got a millisecond buffer size e.g. "ms1000" for live input */
    mdefloat bufsize = atof(s->s_name + 2);
    mdeGranular_tildeSetF(x, bufsize);
    if (mdeGranularInit3(&x->x_g, NULL, bufsize,
                         ms2samples((mdefloat)sys_getsr(),
                                    bufsize))
        < 0)
      pd_error(x, "mdeGranular~: couldn't init Granular object");
  }
  else { /* static buffer */
    x->x_arrayname = s;
    a = (t_garray *)pd_findbyclass(x->x_arrayname, garray_class);
    if (!a) {
      if (*s->s_name) 
        pd_error(x, "mdeGranular~: %s: no such array",
                 x->x_arrayname->s_name);
      samples = NULL;
    }
    /* here we get the samples and their number */
    /* MDE Fri Feb 28 18:58:21 2020 -- see
       https://download.puredata.info/dev/CodeSnippets#garraygetfloat */ 
    /* else if (!garray_getfloatarray(a, &nsamples, &samples)) { */
    else if (!garray_getfloatwords(a, &nsamples, (t_word**)&samples)) {      
      error("%s: bad template for mdeGranular~", x->x_arrayname->s_name);
      samples = NULL;
    }
    else {                 /* success!! */
      if (mdeGranularInit3(&x->x_g, samples, samples2ms(srate, nsamples),
                           (mdefloat)nsamples)
          < 0)
        pd_error(x, "mdeGranular~: couldn't init Granular object");
      garray_usedindsp(a);
    }
  }
}

/*****************************************************************************/

/** This is called every 64 samples or whatever the tick size is. */

t_int *mdeGranular_tildePerform(t_int *w)
{
  t_mdeGranular_tilde* x = (t_mdeGranular_tilde*)(w[1]);
  mdefloat* in = (mdefloat*)(w[2]);
  long nsamps = (long)(w[3]);
  mdeGranular* g = &x->x_g;

  if (g->live && x->x_liverunning && g->status)
    mdeGranularCopyInputSamples(g, in, nsamps);

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
  return w + 4;
}

/*****************************************************************************/

/** This gets called third, when the audio engine starts (after _new!). */

void mdeGranular_tildeDSP(t_mdeGranular_tilde *x, t_signal **sp)
{
  int i;
  /* numChannels has already been set in mdeGranularInit1! */
  mdeGranular* g = &x->x_g;
  int nchan = g->numChannels;
  mdefloat** chbufs = mdeCalloc(nchan, sizeof(mdefloat*), 
                                "mdeGranular_tildeDSP", g->warnings);

  for (i = 0; i < nchan; ++i)
    /* sp[0] is the input of course, so the first output is sp[1] */
    chbufs[i] = sp[i + 1]->s_vec;
  mdeGranularInit2(g, sp[0]->s_n, (mdefloat)DEFAULT_RAMP_LEN, chbufs);
  mdeGranular_tildeSet(x, x->x_arrayname);
  /* the second arg specifies how many elements of the w array arg to the
   * perform routine we can access and the remaining args are the objects that
   * will be those elements. */
  dsp_add(mdeGranular_tildePerform, 3, x, sp[0]->s_vec, sp[0]->s_n);
  mdeFree(chbufs);
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

/** This gets called when a list is sent to the object */

void mdeGranular_tildeList(t_mdeGranular_tilde *x, t_symbol *s,
                            int argc, t_atom *argv)
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
  mdeGranularFree(&x->x_g);
}

/*****************************************************************************/

/** This method is called first. */

void mdeGranular_tilde_setup(void)
{
  mdeGranular_tildeClass = 
    class_new(gensym("mdeGranular~"),
              (t_newmethod)mdeGranular_tildeNew, 
              (t_method)mdeGranular_tildeFree,
              sizeof(t_mdeGranular_tilde), 0,
              A_DEFFLOAT, A_DEFFLOAT, 0);
  CLASS_MAINSIGNALIN(mdeGranular_tildeClass, t_mdeGranular_tilde, x_f);
  class_addmethod(mdeGranular_tildeClass, (t_method)mdeGranular_tildeDSP,
                  gensym("dsp"), 0);
  class_addmethod(mdeGranular_tildeClass,
                  (t_method)mdeGranular_tildeLivestart,
                  gensym("livestart"), 0);
  class_addmethod(mdeGranular_tildeClass,
                  (t_method)mdeGranular_tildeLivestop,
                  gensym("livestop"), 0);
  class_addmethod(mdeGranular_tildeClass,
                  (t_method)mdeGranular_tildePrint,
                  gensym("print"), 0);
  class_addmethod(mdeGranular_tildeClass, (t_method)mdeGranular_tildeSet,
                  gensym("set"), A_DEFSYM, 0);
  class_addmethod(mdeGranular_tildeClass, (t_method)mdeGranular_tildeSetF,
                  gensym("setms"), A_DEFSYM, 0);
  class_addmethod(mdeGranular_tildeClass, (t_method)mdeGranular_tildeRampType,
                  gensym("RampType"), A_DEFSYM, 0);
  class_addmethod(mdeGranular_tildeClass, 
                  (t_method)mdeGranular_tildeRampLenMS,
                  gensym("RampLenMS"), A_DEFFLOAT, 0);
  class_addmethod(mdeGranular_tildeClass, 
                  (t_method)mdeGranular_tildeMaxVoices,
                  gensym("MaxVoices"), A_DEFFLOAT, 0);
  class_addmethod(mdeGranular_tildeClass, 
                  (t_method)mdeGranular_tildeActiveVoices,
                  gensym("ActiveVoices"), A_DEFFLOAT, 0);
  /* to couple an inlet to a method */
  class_addmethod(mdeGranular_tildeClass, 
                  (t_method)mdeGranular_tildeTranspositionOffsetST,
                  gensym("TranspositionOffsetST"), A_DEFFLOAT, 0);
  class_addmethod(mdeGranular_tildeClass, 
                  (t_method)mdeGranular_tildeGrainLengthMS,
                  gensym("GrainLengthMS"), A_DEFFLOAT, 0);
  class_addmethod(mdeGranular_tildeClass, 
                  (t_method)mdeGranular_tildeGrainLengthDeviation,
                  gensym("GrainLengthDeviation"), A_DEFFLOAT, 0);
  class_addmethod(mdeGranular_tildeClass, 
                  (t_method)mdeGranular_tildeSamplesStartMS,
                  gensym("SamplesStartMS"), A_DEFFLOAT, 0);
  class_addmethod(mdeGranular_tildeClass, 
                  (t_method)mdeGranular_tildeSamplesEndMS,
                  gensym("SamplesEndMS"), A_DEFFLOAT, 0);
  class_addmethod(mdeGranular_tildeClass, (t_method)mdeGranular_tildeDensity,
                  gensym("Density"), A_DEFFLOAT, 0);
  class_addmethod(mdeGranular_tildeClass, (t_method)mdeGranular_tildeGrainAmp,
                  gensym("GrainAmp"), A_DEFFLOAT, 0);
  class_addmethod(mdeGranular_tildeClass, (t_method)mdeGranular_tildeOn,
                  gensym("on"), 0);
  class_addmethod(mdeGranular_tildeClass, (t_method)mdeGranular_tildeOff,
                  gensym("off"), 0);
  class_addmethod(mdeGranular_tildeClass, 
                  (t_method)mdeGranular_tildeActiveChannels,
                  gensym("ActiveChannels"), A_DEFFLOAT, 0);
  /* MDE Fri Feb 21 10:52:03 2020 */
  class_addmethod(mdeGranular_tildeClass,
                  (t_method)mdeGranular_tildeDoGrainDelays,
                  gensym("DoGrainDelays"), 0);
  class_addmethod(mdeGranular_tildeClass, (t_method)mdeGranular_tildeSmoothMode,
                  gensym("SmoothMode"), 0);
  class_addmethod(mdeGranular_tildeClass,
                  (t_method)mdeGranular_tildeSetLiveBufferSize, 
                  gensym("MaxLiveBufferMS"),  A_DEFFLOAT, 0);
  class_addmethod(mdeGranular_tildeClass, (t_method)mdeGranular_tildeOctaveSize,
                  gensym("OctaveSize"), A_DEFFLOAT, 0);
  class_addmethod(mdeGranular_tildeClass,
                  (t_method)mdeGranular_tildeOctaveDivisions,
                  gensym("OctaveDivisions"), A_DEFFLOAT, 0);
  class_addmethod(mdeGranular_tildeClass, (t_method)mdeGranular_tildeWarnings,
                  gensym("Warnings"), A_DEFFLOAT, 0);
  class_addmethod(mdeGranular_tildeClass, (t_method)mdeGranular_tildePortion,
                  gensym("Portion"), A_DEFFLOAT, A_DEFFLOAT, 0);
  class_addmethod(mdeGranular_tildeClass,
                  (t_method)mdeGranular_tildePortionPosition,
                  gensym("PortionPosition"), A_DEFFLOAT, 0);
  class_addmethod(mdeGranular_tildeClass,
                  (t_method)mdeGranular_tildePortionWidth,
                  gensym("PortionWidth"), A_DEFFLOAT, 0);
  class_addmethod(mdeGranular_tildeClass,
                  (t_method)mdeGranular_tildeBufferGrainRamp,
                  gensym("BufferGrainRamp"),
                  A_DEFSYM, A_DEFFLOAT, A_DEFFLOAT, 0);
  class_addlist(mdeGranular_tildeClass, mdeGranular_tildeList);
  class_addbang(mdeGranular_tildeClass, mdeGranular_tildeBang);
  mdeGranularWelcome();
}

/*****************************************************************************/

#endif

/*****************************************************************************/

/* EOF mdeGranular~pd.c */
