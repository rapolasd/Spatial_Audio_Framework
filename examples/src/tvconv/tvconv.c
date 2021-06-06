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
 * @file tvconv.c
 * @brief A time-varying multi-channel convolver
 * @author Rapolas Daugintis
 * @date 18.11.2020
 */

#include "tvconv.h"
#include "tvconv_internal.h"

void tvconv_create
(
    void** const phTVCnv
)
{
    tvconv_data* pData = (tvconv_data*)malloc1d(sizeof(tvconv_data));
    *phTVCnv = (void*)pData;

    printf(SAF_VERSION_LICENSE_STRING);

    /* Default user parameters */
    pData->nInputChannels = 1;
    pData->enablePartitionedConv = 0;

    /* internal values */
    pData->hostBlockSize = -1; /* force initialisation */
    pData->inputFrameTD = NULL;
    pData->outputFrameTD = NULL;
    pData->hMatrixConvs = NULL;
    pData->irs = NULL;
    pData->reInitFilters = 1;
    pData->nIrChannels = 0;
    pData->ir_length = 0;
    pData->ir_fs = 0;
    //pData->input_wav_length = 0;
    pData->nOutputChannels = 0;

    /* set FIFO buffers */
    pData->FIFO_idx = 0;
    memset(pData->inFIFO, 0, MAX_NUM_CHANNELS*MAX_FRAME_SIZE*sizeof(float));
    memset(pData->outFIFO, 0, MAX_NUM_CHANNELS*MAX_FRAME_SIZE*sizeof(float));
    
    /* positions */
    pData->positions = NULL;
    pData->positions_Last = NULL;
    pData->nPositions = 0;
    pData->position_idx = 0;
    pData->position_idx_Last = 0;
    pData->position_idx_Last2 = 0;
    for (int d = 0; d < NUM_DIMENSIONS; d++){
        pData->position[d] = 0;
        pData->minDimensions[d] = 0;
        pData->maxDimensions[d] = 0;
    }
    /* flags/status */
    pData->progressBar0_1 = 0.0f;
    pData->progressBarText = malloc1d(PROGRESSBARTEXT_CHAR_LENGTH*sizeof(char));
    strcpy(pData->progressBarText,"");
    pData->codecStatus = CODEC_STATUS_NOT_INITIALISED;
    pData->procStatus = PROC_STATUS_NOT_ONGOING;
}

void tvconv_destroy
(
    void** const phTVCnv
)
{
    tvconv_data* pData = (tvconv_data*)(*phTVCnv);
    
    if (pData != NULL){
        /* not safe to free memory during intialisation/processing loop */
        while (pData->codecStatus == CODEC_STATUS_INITIALISING ||
               pData->procStatus == PROC_STATUS_ONGOING){
            SAF_SLEEP(10);
        }
        
        free(pData->inputFrameTD);
        free(pData->outputFrameTD);
        free(pData->irs);
        free(pData->positions);
        free(pData->positions_Last);
        for (int n = 0; n < pData->nPositions; n++){
            saf_matrixConv_destroy(&(pData->hMatrixConvs[n]));
        }
        free(pData->hMatrixConvs);
        free(pData);
        pData = NULL;
    }
}

void tvconv_init
(
    void* const hTVCnv,
    int sampleRate,
    int hostBlockSize
)
{
    tvconv_data *pData = (tvconv_data*)(hTVCnv);

    pData->host_fs = sampleRate;

    if(pData->hostBlockSize != hostBlockSize){
        pData->hostBlockSize = hostBlockSize;
        pData->hostBlockSize_clamped = SAF_CLAMP(pData->hostBlockSize, MIN_FRAME_SIZE, MAX_FRAME_SIZE);
        pData->reInitFilters = 1;
        tvconv_setCodecStatus(hTVCnv, CODEC_STATUS_NOT_INITIALISED);
    }
    
    tvconv_checkReInit(hTVCnv);
}

void tvconv_process
(
    void  *  const hTVCnv,
    float ** const inputs,
    float ** const outputs,
    int            nInputs,
    int            nOutputs,
    int            nSamples
)
{
    tvconv_data *pData = (tvconv_data*)(hTVCnv);
    int s, ch, i;
    int numInputChannels, numOutputChannels;
 
    tvconv_checkReInit(hTVCnv);
    pData->procStatus = PROC_STATUS_ONGOING;
    /* prep */
    numInputChannels = pData->nInputChannels;
    numOutputChannels = pData->nOutputChannels;

    for(s=0; s<nSamples; s++){
        /* Load input signals into inFIFO buffer */
        for(ch=0; ch<SAF_MIN(SAF_MIN(nInputs,numInputChannels),MAX_NUM_CHANNELS); ch++)
            pData->inFIFO[ch][pData->FIFO_idx] = inputs[ch][s];
        for(; ch<numInputChannels; ch++) /* Zero any channels that were not given */
            pData->inFIFO[ch][pData->FIFO_idx] = 0.0f;

        /* Pull output signals from outFIFO buffer */
        for(ch=0; ch<SAF_MIN(SAF_MIN(nOutputs, numOutputChannels),MAX_NUM_CHANNELS); ch++)
            outputs[ch][s] = pData->outFIFO[ch][pData->FIFO_idx];
        for(; ch<nOutputs; ch++) /* Zero any extra channels */
            outputs[ch][s] = 0.0f;

        /* Increment buffer index */
        pData->FIFO_idx++;

        /* Process frame if inFIFO is full and filters are loaded and saf_matrixConv_apply is ready for it */
        if (pData->FIFO_idx >= pData->hostBlockSize_clamped && pData->reInitFilters == 0 &&
            pData->codecStatus == CODEC_STATUS_INITIALISED) {
            pData->FIFO_idx = 0;

            /* Load time-domain data */
            for(i=0; i < numInputChannels; i++)
                utility_svvcopy(pData->inFIFO[i], pData->hostBlockSize_clamped, pData->inputFrameTD[i]);

            /* Apply matrix convolution */
            if(pData->hMatrixConvs != NULL && pData->ir_length>0)
                saf_matrixConv_apply(pData->hMatrixConvs[pData->position_idx],
                                     FLATTEN2D(pData->inputFrameTD),
                                     FLATTEN2D(pData->outputFrameTD));
            /* if the matrix convolver handle has not been initialised yet (i.e. no filters have been loaded) then zero the output */
            else
                memset(FLATTEN2D(pData->outputFrameTD), 0, MAX_NUM_CHANNELS * (pData->hostBlockSize_clamped)*sizeof(float));

            /* copy signals to output buffer */
            for (i = 0; i < SAF_MIN(numOutputChannels, MAX_NUM_CHANNELS); i++)
                utility_svvcopy(pData->outputFrameTD[i], pData->hostBlockSize_clamped, pData->outFIFO[i]);
        }
        else if(pData->FIFO_idx >= pData->hostBlockSize_clamped){
            /* clear outFIFO if codec was not ready */
            pData->FIFO_idx = 0;
            memset(pData->outFIFO, 0, MAX_NUM_CHANNELS*MAX_FRAME_SIZE*sizeof(float));
        }
    }
    pData->procStatus = PROC_STATUS_NOT_ONGOING;
}

/*sets*/

void tvconv_refreshParams(void* const hTVCnv)
{
    tvconv_data *pData = (tvconv_data*)(hTVCnv);
    pData->reInitFilters = 1;
    //tvconv_setCodecStatus(hTVCnv, CODEC_STATUS_NOT_INITIALISED);
}

void tvconv_checkReInit(void* const hTVCnv)
{
    tvconv_data *pData = (tvconv_data*)(hTVCnv);
    
    while (pData->procStatus == CODEC_STATUS_INITIALISING){
        SAF_SLEEP(10);
    }
    /* reinitialise if needed */
    if ((pData->reInitFilters == 1) && (pData->irs != NULL)) {
        pData->reInitFilters = 2;
//    if ((pData->codecStatus == CODEC_STATUS_NOT_INITIALISED) && (pData->irs != NULL)) {
        for (int n = 0; n < pData->nPositions; n++){
            saf_matrixConv_destroy(&(pData->hMatrixConvs[n]));
            pData->hMatrixConvs[n] = NULL;
        }
        
        /* if length of the loaded sofa file was not divisable by the specified number of inputs, then the handle remains NULL,
         * and no convolution is applied */
        pData->hostBlockSize_clamped = SAF_CLAMP(pData->hostBlockSize, MIN_FRAME_SIZE, MAX_FRAME_SIZE);
        if(pData->ir_length>0){
            for (int n = 0; n < pData->nPositions; n++){
                saf_matrixConv_create(&(pData->hMatrixConvs[n]),
                                      pData->hostBlockSize_clamped, /*pData->hostBlockSize,*/
                                      pData->irs[n],
                                      pData->ir_length,
                                      pData->nInputChannels,
                                      pData->nOutputChannels,
                                      pData->enablePartitionedConv);
            }
        }

        /* Resize buffers */
        pData->inputFrameTD  = (float**)realloc2d((void**)pData->inputFrameTD, MAX_NUM_CHANNELS, pData->hostBlockSize_clamped, sizeof(float));
        pData->outputFrameTD = (float**)realloc2d((void**)pData->outputFrameTD, MAX_NUM_CHANNELS, pData->hostBlockSize_clamped, sizeof(float));
        memset(FLATTEN2D(pData->inputFrameTD), 0, MAX_NUM_CHANNELS*(pData->hostBlockSize_clamped)*sizeof(float));

        /* reset FIFO buffers */
        pData->FIFO_idx = 0;
        memset(pData->inFIFO, 0, MAX_NUM_CHANNELS*MAX_FRAME_SIZE*sizeof(float));
        memset(pData->outFIFO, 0, MAX_NUM_CHANNELS*MAX_FRAME_SIZE*sizeof(float));

        pData->reInitFilters = 0;
        pData->codecStatus = CODEC_STATUS_INITIALISED;
    }
}

void tvconv_setFiltersAndPositions
(
    void* const hTVCnv
)
{
    tvconv_data* pData = (tvconv_data*) hTVCnv;
    
    if (pData->codecStatus != CODEC_STATUS_NOT_INITIALISED)
        return; /* re-init not required, or already happening */
    while (pData->procStatus == PROC_STATUS_ONGOING){
        /* re-init required, but we need to wait for the current processing loop to end */
        pData->codecStatus = CODEC_STATUS_INITIALISING; /* indicate that we want to init */
        SAF_SLEEP(10);
    }
    
    /* for progress bar */
    pData->codecStatus = CODEC_STATUS_INITIALISING;
    strcpy(pData->progressBarText,"Initialising");
    pData->progressBar0_1 = 0.0f;
    
    SAF_SOFA_ERROR_CODES error;
    saf_sofa_container sofa;
    int i;
    if(pData->sofa_filepath!=NULL){
        strcpy(pData->progressBarText,"Opening SOFA file");
        pData->progressBar0_1 = 0.2f;
        error = saf_sofa_open(&sofa, pData->sofa_filepath);
        
        if(error==SAF_SOFA_OK){
            
            strcpy(pData->progressBarText,"Loading IRs");
            pData->progressBar0_1 = 0.5f;
            
            pData->ir_fs = (int)sofa.DataSamplingRate;
            pData->ir_length = sofa.DataLengthIR;
            pData->nIrChannels = sofa.nReceivers;
            pData->nPositions = sofa.nListeners;
            
            pData->hMatrixConvs = realloc1d(pData->hMatrixConvs, pData->nPositions*sizeof(void*));
            for (int n=0; n < pData->nPositions; n++)
                pData->hMatrixConvs[n]=NULL;
            
            pData->irs = (float**)realloc2d((void**)pData->irs, pData->nPositions, pData->nIrChannels*pData->ir_length, sizeof(float));
            int tmp_length = pData->nIrChannels * pData->ir_length;
            for(i=0; i<pData->nPositions; i++){
                memcpy(pData->irs[i], &(sofa.DataIR[i*tmp_length]), tmp_length*sizeof(float));
            }
            
            strcpy(pData->progressBarText,"Loading positions");
            pData->progressBar0_1 = 0.8f;
            
            pData->positions = (vectorND*)realloc1d((void*)pData->positions, pData->nPositions*sizeof(vectorND));
            memcpy(pData->positions, sofa.ListenerPosition, pData->nPositions*sizeof(vectorND));
            

        }
    }
    
    pData->nOutputChannels = SAF_MIN(pData->nIrChannels, MAX_NUM_CHANNELS);
    saf_sofa_close(&sofa);
    tvconv_setMinMaxDimensions(hTVCnv);
    pData->codecStatus = CODEC_STATUS_INITIALISED;
    pData->reInitFilters = 1;
    
    /* done! */
    strcpy(pData->progressBarText,"Done!");
    pData->progressBar0_1 = 1.0f;

}

void tvconv_setEnablePart(void* const hTVCnv, int newState)
{
    tvconv_data *pData = (tvconv_data*)(hTVCnv);
    if(pData->enablePartitionedConv!=newState){
        pData->enablePartitionedConv = newState;
        pData->reInitFilters = 1;
    }
}

void tvconv_setSofaFilePath(void* const hTVCnv, const char* path)
{
    tvconv_data *pData = (tvconv_data*)(hTVCnv);
    
    pData->sofa_filepath = malloc1d(strlen(path) + 1);
    strcpy(pData->sofa_filepath, path);
    pData->codecStatus = CODEC_STATUS_NOT_INITIALISED;
    tvconv_setFiltersAndPositions(hTVCnv);
    pData->reInitFilters = 1;  // re-init and re-calc
}

void tvconv_setPosition(void* const hTVCnv, int dim, float position){
    tvconv_data *pData = (tvconv_data*)(hTVCnv);
    assert(dim >= 0 && dim < NUM_DIMENSIONS);
    pData->position[dim] = position;
    tvconv_findNearestNeigbour(hTVCnv);
}

void tvconv_setNumInputChannels(void* const hTVCnv, int newValue)
{
    tvconv_data *pData = (tvconv_data*)(hTVCnv);
    pData->nInputChannels = 1;
//    pData->nInputChannels = CLAMP(newValue, 1, MAX_NUM_CHANNELS);
//    pData->nIrChannels = (pData->nOutputChannels) * (pData->nInputChannels);
//    if((pData->nOutputChannels > 0) && (pData->input_wav_length % pData->nInputChannels == 0))
//        pData->filter_length = (pData->input_wav_length) / (pData->nInputChannels);
//    else
//        pData->filter_length = 0;
    pData->reInitFilters = 1;
}

/*gets*/

int tvconv_getEnablePart(void* const hTVCnv)
{
    tvconv_data *pData = (tvconv_data*)(hTVCnv);
    return pData->enablePartitionedConv;
}

int tvconv_getNumInputChannels(void* const hTVCnv)
{
    tvconv_data *pData = (tvconv_data*)(hTVCnv);
    return pData->nInputChannels;
}

int tvconv_getNumOutputChannels(void* const hTVCnv)
{
    tvconv_data *pData = (tvconv_data*)(hTVCnv);
    return pData->nOutputChannels;
}

int tvconv_getHostBlockSize(void* const hTVCnv)
{
    tvconv_data *pData = (tvconv_data*)(hTVCnv);
    return pData->hostBlockSize;
}

int tvconv_getNIRs(void* const hTVCnv)
{
    tvconv_data *pData = (tvconv_data*)(hTVCnv);
    return pData->nIrChannels;
}

int tvconv_getNPositions(void* const hTVCnv)
{
    tvconv_data *pData = (tvconv_data*)(hTVCnv);
    return pData->nPositions;
}
int tvconv_getPositionIdx(void* const hTVCnv)
{
    tvconv_data *pData = (tvconv_data*)(hTVCnv);
    return pData->position_idx;
}

float tvconv_getPosition(void* const hTVCnv, int dim)
{
    tvconv_data *pData = (tvconv_data*)(hTVCnv);
    assert(dim >= 0 && dim < NUM_DIMENSIONS);
    return (float) pData->position[dim];
}

float tvconv_getMinDimension(void* const hTVCnv, int dim)
{
    tvconv_data *pData = (tvconv_data*)(hTVCnv);
    assert(dim >= 0 && dim < NUM_DIMENSIONS);
    return (float) pData->minDimensions[dim];
}

float tvconv_getMaxDimension(void* const hTVCnv, int dim)
{
    tvconv_data *pData = (tvconv_data*)(hTVCnv);
    assert(dim >= 0 && dim < NUM_DIMENSIONS);
    return (float) pData->maxDimensions[dim];
}

int tvconv_getIRLength(void* const hTVCnv)
{
    tvconv_data *pData = (tvconv_data*)(hTVCnv);
    return pData->ir_length;
}

int tvconv_getIRFs(void* const hTVCnv)
{
    tvconv_data *pData = (tvconv_data*)(hTVCnv);
    return pData->ir_fs;
}

int tvconv_getHostFs(void* const hTVCnv)
{
    tvconv_data *pData = (tvconv_data*)(hTVCnv);
    return pData->host_fs;
}

int tvconv_getProcessingDelay(void* const hTVCnv)
{
    tvconv_data *pData = (tvconv_data*)(hTVCnv);
    return pData->hostBlockSize_clamped;
}

CODEC_STATUS tvconv_getCodecStatus(void* const hTVCnv)
{
    tvconv_data *pData = (tvconv_data*)(hTVCnv);
    return pData->codecStatus;
}

void tvconv_test(void* const hTVCnv)
{
    tvconv_setSofaFilePath(hTVCnv, "/Users/dauginr1/Documents/Special_Assignment/rir-interpolation-vst/rirs_unprocessed_M3_3D_rndLP.sofa");
    //tvconv_setFiltersAndPositions(hTVCnv);
    tvconv_data* pData = (tvconv_data*) hTVCnv;
    
//    pData->position = malloc1d(sizeof(vectorND));
    //pData->nPositions = 3;
    pData->position[0] = 0.04;
    pData->position[1] = 0.04;
    pData->position[2] = 0.05;
    
    tvconv_findNearestNeigbour(hTVCnv);
    printf("Nearest neighbour index: %i\n", pData->position_idx);
}
