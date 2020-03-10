/******************************************************************************
 *
 * File:             mdeGranular~.h
 *
 * Author:           Michael Edwards - m@michael-edwards.org - 
 *                   http://www.michael-edwards.org
 *
 * Date:             June 13th 2003
 *
 * $$ Last modified:  14:56:03 Tue Mar 10 2020 CET
 *
 * Purpose:          Header file definitions for external for multi-channel, 
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

/* define MAXMSP via the compiler rather than here */
/* #define MAXMSP */

#ifdef MAXMSP
#include "ext.h"
#include "ext_obex.h"
#include "z_dsp.h"
#include "buffer.h"
#define VERSION "1.2 (Max API 8.0.3)"
#endif

#ifdef PD
#include "m_pd.h"
#define VERSION "1.2"
#endif

#ifdef WIN32
#define inline __inline
#else
#define inline inline
#endif

/*****************************************************************************/

/** To ensure floating point size compatibility, the portable algorithm will
 *  use mdefloat as its floating point type, which is here typedefd to be the
 *  float size of the system we are integrating into. 
 *  */

#ifdef PD
/* Here the t_float is from PD. */
typedef t_float mdefloat;
#endif
#ifdef MAXMSP
/* MDE Wed Sep 18 18:16:48 2013 -- changing to doubles with dsp64
   typedef t_float mdefloat; */
typedef double mdefloat;
#endif

/*****************************************************************************/

/** Enumeration for holding the status of the granulator and the grains. */
typedef enum 
  { OFF, ON, STARTING, STOPPING, ACTIVE, INACTIVE, SKIPGRAIN }
  t_status;

/*****************************************************************************/

/** the maximum number of transpositions the granulator can handle */
#define MAXTRANSPOSITIONS 256

/* The minimum size in millisecs of the buffer used for live granulation, */
#define MINLIVEBUFSIZE 6.0

#define DEFAULT_RAMP_TYPE "HANNING"
#define DEFAULT_RAMP_LEN 10
#define RAMPLENMINMS 0.5

/* to suppress warnings about unused arguments */
#define UNUSED(x) (void)(x)

/*****************************************************************************/

typedef struct _mdeGranularGrain
{
  /** grain length in samples */
  long length;        
  /** start sample */
  mdefloat start;     
  /** end sample */
  mdefloat end;       
  /** at which value of icurrent does the ramp up end */
  long endRampUp;     
  /** at which value of icurrent does the ramp down start */
  long startRampDown; 
  /** current sample index (partial) into the sample buffer */
  mdefloat current;   
  /** sample counter for the grain (from 0 to length) */
  long icurrent;      
  /** index into ramp */
  long rampi;         
  /** sample increment */
  mdefloat inc;       
  /** 1 when we're playing backwards, 0 if not */
  char backwards;
  /** whether the grain should be played or not or whether it's
   *  stopping/starting */  
  t_status status;
  /** 19/7/04: Added this slot to take over whether the grain is
   *  active or inactive rather than setting the status slot (which
   *  could be trying to indicate that it's stopping or starting */
  t_status activeStatus;
  /** which channel the grain will be played on */
  int channel; 
  /** whether to introduce a delay the next time the grain is initialised.
   * 4/4/08:  0 = no delay; 1 = random delay; anything else is the number of
   * samples to delay */
  int doDelay;
  /** when the grains are initialized at the beginning, we make it wait for a
   *  while until it actually starts output */
  long firstDelay;
  /** this is the counter up to firstDelay */
  long firstDelayCounter;
} mdeGranularGrain;

/*****************************************************************************/

/** Wrapper structure to hold the grain voices and other data relating
 *  to the overal granulation process.
 *
 *  Before using this structure (i.e. calling mdeGranularGo), the
 *  initialization functions mdeGranularInit1, mdeGranularInit2 and
 *  mdeGranularInit3 must be called.
 * */

typedef struct _mdeGranular
{
  mdefloat samplingRate;
  /** the max number of voices (layers) of granulation requested */
  int maxVoices;
  /** the number of those voices that are presently active */
  int activeVoices;
  /** the semitone offset added to transpositions */
  mdefloat transpositionOffsetST;
  /** the above converted to src */
  mdefloat transpositionOffset;
  /** the number of transpositions in semitones given as a list to the object
   */ 
  int numTranspositions; 
  /** an array of transpositions as given in semitones to the object */
  mdefloat transpositions[MAXTRANSPOSITIONS] ;
  /** an array of transpositions in src (1 no transposition, 0.5 octave lower,
   *  2 octave above), convereted from above */
  mdefloat srcs[MAXTRANSPOSITIONS];
  /** the grain length in milliseconds, as given to the object */
  mdefloat grainLengthMS;
  /** the grain length in samples, converted from above */
  int grainLength;
  /** percentage deviation for grain length: actual grain length will be
   * randomised within grainLength +/- deviation */
  mdefloat grainLengthDeviation;
  /** The name of the buffer to be granulated */
  char BufferName[128];
  /** the number of output channels */
  int numChannels;
  /** the number of output channels currently sending grains */
  int activeChannels;
  /** where MSP/PD wants us to write each individual output channel
   * i.e. the signal outlets. N.B. Although it would seem that this
   * should be external to our object, we need access to all the
   * channels because a grain will change channel upon
   * reinitialization (i.e. when it is over), probably in the middle
   * of a dsp tick. */
  mdefloat** channelBuffers;
  /** where maxmsp/PD stores the incoming signal */
  mdefloat* signalIn;
  /** how many samples to output each time mdeGranularGo is called */
  long nOutputSamples;
  /** array of grain structures, one for each voice */
  mdeGranularGrain* grains;
  /** a sample buffer for storing live incoming samples; samples will
   *  point to this when we are granulating live. */
  mdefloat* theSamples;
  /** the samples to granulate, whether live or from a buffer (always
   *  a mono signal). this is only a pointer; the actual allocated buffer is
   *  theSamples */  
  mdefloat* samples;
  /** how many samples there are in the buffer. NB If live
   *  granulation, this will actually be the size of the circular
   *  buffer into which samples are read (i.e. set in Init3()), not the
   *  actual buffer allocated by SetLiveBufferSize(), which will
   *  probably be larger. */
  long nBufferSamples;
  /** this is the actual number of samples allocated for in the live
   *  buffer */
  long nAllocatedBufferSamples;
  /** this is the same in millisecs */
  mdefloat AllocatedBufferMS;
  /** how many millisecs of samples there are in the buffer */
  mdefloat BufferSamplesMS;
  /** where to start in the samples in millisecs */
  mdefloat samplesStartMS;
  /** where to start in the samples in samples */
  long samplesStart;
  /** where to end in the samples in millisecs */
  mdefloat samplesEndMS;
  /** where to end in the samples in samples */
  long samplesEnd;
  /** we do a straight ramp up, this is the length of such in
   *  milliseconds... */ 
  mdefloat rampLenMS;
  /** ...and this is it in samples */
  long rampLenSamples;
  /** this is the array of scalers for the ramp up... */
  mdefloat* rampUp;
  /** ...and ramp down */
  mdefloat* rampDown;
  /** what percentage of grains should actually produce output. This
   *  is a percentage that will be used to randomly switch a grain on
   *  when it's over this threshold. */
  mdefloat density;
  /** whether the granulator should produce output or not (i.e. if this is 0,
   *  then it is silent) */
  t_status status;
  /** amplitude scaler applied to all grains */
  mdefloat grainAmp;
  /** the last grain amp accepted */
  mdefloat lastGrainAmp;
  /** the grain amp we're aiming to reach */
  mdefloat targetGrainAmp;
  /** the increment needed to get from lastGrainAmp to targetGrainAmp over
   *  a tick's worth of samples */
  mdefloat grainAmpInc;
  /** we need a tick's worth of grainAmps when moving from lastGrainAmp to
   *  targetGrainAmp so here's storage for them */
  mdefloat* grainAmps;
  /** index into rampDown or rampUp for doing a quick fade in/out when the
   *  granulator is stopped. */
  long statusRampIndex;
  /** we can granulate a static buffer of samples or a live incoming signal,
   *  this will be 0 or 1 respectively */
  char live;
  /** we store the incoming samples in |samples| which is then a circular
   *  buffer; this is the index to the oldest sample. */
  long liveIndex;
  /** the type of window to use for ramping: hamming, blackman etc. */
  char* rampType;
  /** when doing transposition, what octave size and number of divisions are we
   *  working with (default 2 and 12) */
  mdefloat octaveSize;
  mdefloat octaveDivisions;
  /** whether we should print stuff to the max window whilst running. */
  char warnings;
  /** 31/8/10: just holding positions for the new data associated with the
   *  Portion message */ 
  mdefloat portionPosition;
  mdefloat portionWidth;
} mdeGranular;

/*****************************************************************************/
/** The pd granular object which simply contains the mdeGranular object and a
 *  couple of other data necessary for the pd side of things.
 *  */
#ifdef PD
typedef struct _mdeGranular_tilde {
  t_object x_obj;
  t_symbol *x_arrayname;
  mdeGranular x_g;
  /* whether we're recording the incoming signal or not */
  char x_liverunning;
  /* all classes that have a signal in need a float member in case a single
   * float instead of a signal is given (apparently). */
  t_float x_f;
} t_mdeGranular_tilde;
#endif

#ifdef MAXMSP
/** the object for max/pd's purposes */
typedef struct _mdeGranular_tilde {
  t_pxobject x_obj;
  t_symbol* x_arrayname;
  mdeGranular x_g;
  /* whether we're recording the incoming signal or not */
  char x_liverunning;
} t_mdeGranular_tilde;
#endif

/*****************************************************************************/

void mdeGranularInitGrains(mdeGranular* g);
int mdeGranularGrainInit(mdeGranularGrain* gg, mdeGranular* parent,
                         int doFirstDelay);
void mdeGranularGrainMixIn(mdeGranularGrain* gg, mdeGranular* g, 
                           mdefloat* where, int howMany);
inline mdefloat st2src(mdefloat st, mdefloat octaveSize, 
                       mdefloat octaveDivisions);
inline long ms2samples(mdefloat samplingRate, mdefloat milliseconds);
inline mdefloat samples2ms(mdefloat samplingRate, int samples);
inline mdefloat between(mdefloat min, mdefloat max);
inline int flip(void);
void mdeGranularMdeFree(mdeGranular* g);
void makeRamps(int rampLen, mdefloat* rampUp, mdefloat* rampDown);
mdefloat randomlyDeviate(mdefloat number, mdefloat maxDeviation);
/* MDE Thu Feb 20 11:39:46 2020 -- 'live' arg doesn't seem to be used at all, so
   removing  */
mdefloat interpolate(mdefloat findex, mdefloat* samples, long numSamples,
                     char backwards); /* , char live);*/
inline int mdeGranularGrainExhausted(mdeGranularGrain* g);
inline mdefloat mdeGranularGrainGetRampVal(mdeGranularGrain* gg, 
                                           mdefloat* rampUp, 
                                           mdefloat* rampDown, long rampLen);
inline void* mdeCalloc(int howmany, size_t size, char* caller, char warn);
inline void mdeFree(void* what);
inline void silence(mdefloat* where, int numSamples);
inline int mdeGranularAtTargetGrainAmp(mdeGranular* g);
int isanum(char *input);
mdefloat* makeWindow(char* type, int size, mdefloat beta, mdefloat* window);
void mdeGranularStoreRampType(mdeGranular* g, char* type);
inline void mdeGranularClearLiveSamples(mdeGranular* g);
void warnGrain2BufferLength(mdeGranular* g);
inline void mdeGranularDoGrainDelays(mdeGranular* g);
mdefloat maxFloat(mdefloat* array, int size);
inline void mdeGranularForceGrainReinit(mdeGranular* g);
int mdeGranularInit1(mdeGranular* g, int maxVoices, int numChannels);
void mdeGranularPrint(mdeGranular* g);
int mdeGranularInit3(mdeGranular* g, mdefloat* samples, mdefloat samplesMS,
                     mdefloat numSamples);
inline void mdeGranularCopyInputSamples(mdeGranular* g, mdefloat* in,
                                        long nsamps);
void mdeGranularGo(mdeGranular* g);
int mdeGranularInit2(mdeGranular* g, long nOutputSamples, mdefloat rampLenMS,
                     mdefloat** channelBuffers);
inline int mdeGranularIsOn(mdeGranular* g);
inline int mdeGranularIsOff(mdeGranular* g);
inline void mdeGranularOn(mdeGranular* g);
void mdeGranularSetTranspositions(mdeGranular* g, int num, mdefloat* list);
void mdeGranularFree(mdeGranular* g);
inline void mdeGranularSetTranspositionOffsetST(mdeGranular* g, mdefloat f);
inline void mdeGranularSetGrainLengthMS(mdeGranular* g, mdefloat f);
inline void mdeGranularSetGrainLengthDeviation(mdeGranular* g, mdefloat f);
inline void mdeGranularSetSamplesStartMS(mdeGranular* g, mdefloat f);
inline void mdeGranularSetSamplesEndMS(mdeGranular* g, mdefloat f);
inline void mdeGranularSetDensity(mdeGranular* g, mdefloat f);
void mdeGranularSetActiveChannels(mdeGranular* g, long l);
void mdeGranularSetWarnings(mdeGranular* g, long l);
inline void mdeGranularSetGrainAmp(mdeGranular* g, mdefloat f);
void mdeGranularSetMaxVoices(mdeGranular* g, mdefloat maxVoices);
void mdeGranularSetActiveVoices(mdeGranular* g, mdefloat activeVoices);
void mdeGranularSetRampLenMS(mdeGranular* g, mdefloat rampLenMS);
void mdeGranularSetRampType(mdeGranular* g, char* type);
void mdeGranularSetLiveBufferSize(mdeGranular* g, mdefloat sizeMS);
inline void mdeGranularSmoothMode(mdeGranular* g);
void mdeGranularOctaveSize(mdeGranular* g, mdefloat size);
void mdeGranularOctaveDivisions(mdeGranular* g, mdefloat divs);
void mdeGranularWelcome(void);
inline void mdeGranularOff(mdeGranular* g);
inline void mdeGranularPortion(mdeGranular* g, mdefloat position, 
                               mdefloat width);
void mdeGranular_tildeSetF(t_mdeGranular_tilde *x, double bufsize);
inline void mdeGranularPortionPosition(mdeGranular* g, mdefloat position);
inline void mdeGranularPortionWidth(mdeGranular* g, mdefloat width);
inline int mdeGranularDidInit(mdeGranular* g);
#ifdef MAXMSP
void mdegranular_tildeUnlockBuffer(t_buffer_ref* buf);
#endif
void mdeGranularClearTheSamples(mdeGranular* g);
long mdeGranularCopyFloatSamples(mdeGranular* g, float* in, long nsamps);
void mdeGranular_tildeSet(t_mdeGranular_tilde *x, t_symbol *s);

void mdeGranular_tildeTranspositionOffsetST(t_mdeGranular_tilde* x, mdefloat f);
void mdeGranular_tildeGrainLengthMS(t_mdeGranular_tilde* x, mdefloat f);
void mdeGranular_tildeGrainLengthDeviation(t_mdeGranular_tilde* x, mdefloat f);
void mdeGranular_tildeSamplesStartMS(t_mdeGranular_tilde* x, mdefloat f);
void mdeGranular_tildeSamplesEndMS(t_mdeGranular_tilde* x, mdefloat f);
void mdeGranular_tildeDensity(t_mdeGranular_tilde* x, mdefloat f);
void mdeGranular_tildeActiveChannels(t_mdeGranular_tilde* x, long l);
void mdeGranular_tildeWarnings(t_mdeGranular_tilde* x, long l);
void mdeGranular_tildeGrainAmp(t_mdeGranular_tilde* x, mdefloat f);
void mdeGranular_tildeMaxVoices(t_mdeGranular_tilde* x, mdefloat f);
void mdeGranular_tildeActiveVoices(t_mdeGranular_tilde* x, mdefloat f);
void mdeGranular_tildeRampLenMS(t_mdeGranular_tilde* x, mdefloat f);
void mdeGranular_tildeRampType(t_mdeGranular_tilde *x, t_symbol *s);
void mdeGranular_tildeOn(t_mdeGranular_tilde *x);
void mdeGranular_tildeOff(t_mdeGranular_tilde *x);
void mdeGranular_tildeSetLiveBufferSize(t_mdeGranular_tilde *x, mdefloat f);
void mdeGranular_tildeDoGrainDelays(t_mdeGranular_tilde *x);
void mdeGranular_tildeSmoothMode(t_mdeGranular_tilde *x);
void mdeGranular_tildeOctaveSize(t_mdeGranular_tilde *x, mdefloat f);
void mdeGranular_tildeOctaveDivisions(t_mdeGranular_tilde *x, mdefloat f);
void mdeGranular_tildePortion(t_mdeGranular_tilde *x, mdefloat position, 
                              mdefloat width);
void mdeGranular_tildePortionPosition(t_mdeGranular_tilde *x,
                                      mdefloat position);
void mdeGranular_tildePortionWidth(t_mdeGranular_tilde *x, mdefloat width);
void mdeGranular_tildeBufferGrainRamp(t_mdeGranular_tilde *x, t_symbol *s,
                                      mdefloat grain_len, mdefloat ramp_len);

/*****************************************************************************/

/* EOF mdeGranular~.h */
