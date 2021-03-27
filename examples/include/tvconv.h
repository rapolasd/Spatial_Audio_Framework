/*
 * Copyright 2020 Leo McCormack
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file tvconv.h
 * @brief A time-varying multi-channel convolver
 * @author Rapolas Daugintis
 * @date 18.11.2020
 */

#ifndef __TVCONV_H_INCLUDED__
#define __TVCONV_H_INCLUDED__

#include "_common.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* ========================================================================== */
/*                               Main Functions                               */
/* ========================================================================== */

/**
 * Creates an instance of tvconv
 *
 * @param[in] phTVCnv (&) address of tvconv handle
 */
void tvconv_create(void** const phTVCnv);

/**
 * Destroys an instance of tvconv
 *
 * @param[in] phTVCnv (&) address of tvconv handle
 */
void tvconv_destroy(void** const phTVCnv);

/**
 * Initialises an instance of tvconv with default settings
 *
 * @param[in] hTVCnv         tvconv handle
 * @param[in] samplerate    Host samplerate.
 * @param[in] hostBlockSize Host frame/block size
 */
void tvconv_init(void* const hTVCnv,
                 int samplerate,
                 int hostBlockSize);

/**
 * Performs the matrix convolution processing
 *
 * @param[in] hTVCnv    tvconv handle
 * @param[in] inputs    Input channel buffers; 2-D array: nInputs x nSamples
 * @param[in] outputs   Output channel buffers; 2-D array: nOutputs x nSamples
 * @param[in] nInputs   Number of input channels
 * @param[in] nOutputs  Number of output channels
 * @param[in] nSamples  Number of samples in 'inputs'/'output' matrices
 */
void tvconv_process(void* const hTVCnv,
                    float** const inputs,
                    float** const outputs,
                    int nInputs,
                    int nOutputs,
                    int nSamples);

/* ========================================================================== */
/*                                Set Functions                               */
/* ========================================================================== */
    
/**
 * Sets all intialisation flags to 1. Re-initialising all settings/variables,
 * as tvconv is currently configured, at next available opportunity.
 */
void tvconv_refreshParams(void* const hTVCnv);

/**
 * Checks whether things have to be reinitialised, and does so if it is needed
 */
void tvconv_checkReInit(void* const hTVCnv);


void tvconv_setFiltersAndPositions(void* const hTVCnv);

/**
 * Enable (1), disable (0), partitioned convolution
 */
void tvconv_setEnablePart(void* const hTVCnv, int newState);

/**
 * Sets the number of input channels.
 *
 * @note The loaded wav data channels are divided by the number of channels
 *       (into equal lengths). These are interpreted by tvconv as the
 *       filters to apply to each input channel to acquire the corresponding
 *       output channel
 */
void tvconv_setNumInputChannels(void* const hTVCnv, int newValue);

void tvconv_setSofaFilePath(void* const hTVCnv, const char* path);

/* ========================================================================== */
/*                                Get Functions                               */
/* ========================================================================== */

/**
 * Returns the processing framesize (i.e., number of samples processed with
 * every _process() call )
 */
int tvconv_getFrameSize(void);

/**
 * Returns a flag indicating whether partitioned convolution is enabled (1) or
 * disabled (0)
 */
int tvconv_getEnablePart(void* const hTVCnv);
    
/**
 * Returns the number input channels
 */
int tvconv_getNumInputChannels(void* const hTVCnv);

/**
 * Returns the number of output channels (the same as the number of channels in
 * the loaded sofa file)
 */
int tvconv_getNumOutputChannels(void* const hTVCnv);

/**
 * Returns the currect host block size
 */
int tvconv_getHostBlockSize(void* const hTVCnv);

/**
 * Returns the number of filters in the loaded wav file (number of outputs
 * multiplied by the number of inputs)
 */
int tvconv_getNIRs(void* const hTVCnv);

/**
 * Returns the current filter length, in samples
 */
int tvconv_getIRLength(void* const hTVCnv);

/**
 * Returns the samplerate of the loaded filters
 */
int tvconv_getIRFs(void* const hTVCnv);

/**
 * Returns the samperate of the host
 */
int tvconv_getHostFs(void* const hTVCnv);

/**
 * Returns the processing delay in samples (may be used for delay compensation
 * features)
 */
int tvconv_getProcessingDelay(void* const hTVCnv);

void tvconv_test(void* const hTVCnv);

#ifdef __cplusplus
} /* extern "C" { */
#endif /* __cplusplus */

#endif /* __TVCONV_H_INCLUDED__ */
