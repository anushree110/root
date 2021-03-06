// @(#)root/tmva/tmva/dnn:$Id$
// Author: Vladimir Ilievski

/**********************************************************************************
 * Project: TMVA - a Root-integrated toolkit for multivariate data analysis       *
 * Package: TMVA                                                                  *
 * Class  : TTensorDataLoader                                                     *
 * Web    : http://tmva.sourceforge.net                                           *
 *                                                                                *
 * Description:                                                                   *
 *      Tensor Data Loader Class                                                  *
 *                                                                                *
 * Authors (alphabetical):                                                        *
 *      Vladimir Ilievski      <ilievski.vladimir@live.com>  - CERN, Switzerland  *
 *                                                                                *
 * Copyright (c) 2005-2015:                                                       *
 *      CERN, Switzerland                                                         *
 *      U. of Victoria, Canada                                                    *
 *      MPI-K Heidelberg, Germany                                                 *
 *      U. of Bonn, Germany                                                       *
 *                                                                                *
 * Redistribution and use in source and binary forms, with or without             *
 * modification, are permitted according to the terms listed in LICENSE           *
 * (http://tmva.sourceforge.net/LICENSE)                                          *
 **********************************************************************************/

#ifndef TMVA_DNN_TENSORDATALOADER
#define TMVA_DNN_TENSORDATALOADER

#include "TMatrix.h"
#include "TMVA/Event.h"

namespace TMVA {
namespace DNN {

//
// Input Data Types
//______________________________________________________________________________
using TensorInput =
   std::tuple<const std::vector<TMatrixT<Double_t>> &, const TMatrixT<Double_t> &, const TMatrixT<Double_t> &>;

using TMVAInput_t = std::vector<Event *>;
using IndexIterator_t = typename std::vector<size_t>::iterator;

/** TTensorBatch
 *
 * Class representing training batches consisting of a vector of matrices as input data
 * and a matrix of output data. The input and output data can be accessed using
 * the GetInput() and GetOutput() member functions.
 *
 * \tparam Architecture_t The underlying architecture.
 */

template <typename Architecture_t>
class TTensorBatch {
public:
   using Matrix_t = typename Architecture_t::Matrix_t;

private:
   std::vector<Matrix_t> fInputTensor; ///< The input tensor batch, one matrix one input.
   Matrix_t fOutputMatrix;             ///< The output matrix representing the ground truth.
   Matrix_t fWeightMatrix;

public:
   TTensorBatch(std::vector<Matrix_t> &, Matrix_t &, Matrix_t &);
   TTensorBatch(const TTensorBatch &) = default;
   TTensorBatch(TTensorBatch &&) = default;
   TTensorBatch &operator=(const TTensorBatch &) = default;
   TTensorBatch &operator=(TTensorBatch &&) = default;

   /** Return the tensor representing the input data */
   std::vector<Matrix_t> &GetInput() { return fInputTensor; }
   /** Return the matrix representing the output data. */
   Matrix_t &GetOutput() { return fOutputMatrix; }
   /** Return the matrix holding the event weights. */
   Matrix_t &GetWeights() { return fWeightMatrix; }
};

template <typename Data_t, typename Architecture_t>
class TTensorDataLoader;

/** TTensorBatchIterator
 *
 * Simple iterator class for the iterations over the training batches in
 * a given data set represented by a TTensorDataLoader object.
 *
 * \tparam Data_t         The input data type.
 * \tparam Architecture_t The underlying architecture type.
 */
template <typename Data_t, typename Architecture_t>
class TTensorBatchIterator {
private:
   TTensorDataLoader<Data_t, Architecture_t> &fTensorDataLoader;
   size_t fBatchIndex;

public:
   TTensorBatchIterator(TTensorDataLoader<Data_t, Architecture_t> &tensorDataLoader, size_t index = 0)
      : fTensorDataLoader(tensorDataLoader), fBatchIndex(index)
   {
      // Nothing to do here.
   }

   TTensorBatch<Architecture_t> operator*() { return fTensorDataLoader.GetTensorBatch(); }
   TTensorBatchIterator operator++()
   {
      fBatchIndex++;
      return *this;
   }
   bool operator!=(const TTensorBatchIterator &other) { return fBatchIndex != other.fBatchIndex; }
};

/** TTensorDataLoader
 *
 * Service class managing the streaming of the training data from the input data
 * type to the accelerator device or the CPU. A TTensorDataLoader object manages
 * a number of host and device buffer pairs that are used in a round-robin manner
 * for the transfer of batches to the device.
 *
 * Each TTensorDataLoader object has an associated batch size and a number of total
 * samples in the dataset. One epoch is the number of buffers required to transfer
 * the complete training set. Using the begin() and end() member functions allows
 * the user to iterate over the batches in one epoch.
 *
 * \tparam Data_t The input data type.
 * \tparam Architecture_t The achitecture class of the underlying architecture.
 */
template <typename Data_t, typename Architecture_t>
class TTensorDataLoader {
private:
   using HostBuffer_t = typename Architecture_t::HostBuffer_t;
   using DeviceBuffer_t = typename Architecture_t::DeviceBuffer_t;
   using Matrix_t = typename Architecture_t::Matrix_t;
   using BatchIterator_t = TTensorBatchIterator<Data_t, Architecture_t>;

   const Data_t &fData; ///< The data that should be loaded in the batches.

   size_t fNSamples;        ///< The total number of samples in the dataset.
   size_t fBatchSize;       ///< The size of a batch.
   size_t fBatchDepth;      ///< The number of matrices in the tensor.
   size_t fBatchHeight;     ///< The number od rows in each matrix.
   size_t fBatchWidth;      ///< The number of columns in each matrix.
   size_t fNOutputFeatures; ///< The number of outputs from the classifier/regressor.
   size_t fBatchIndex;      ///< The index of the batch when there are multiple batches in parallel

   size_t fNStreams;                           ///< Number of buffer pairs.
   std::vector<DeviceBuffer_t> fDeviceBuffers; ///< The device buffers used to keep the input, output and weight data.
   std::vector<HostBuffer_t> fHostBuffers;     ///< The host buffers used to load the input, output and weight data.

   std::vector<size_t> fSampleIndices; ///< Ordering of the samples in the epoch.

public:
   /*! Constructor. */
   TTensorDataLoader(const Data_t &data, size_t nSamples, size_t batchSize, size_t batchDepth, size_t batchHeight,
                     size_t batchWidth, size_t nOutputFeatures, size_t nStreams = 1);

   TTensorDataLoader(const TTensorDataLoader &) = default;
   TTensorDataLoader(TTensorDataLoader &&) = default;
   TTensorDataLoader &operator=(const TTensorDataLoader &) = default;
   TTensorDataLoader &operator=(TTensorDataLoader &&) = default;

   /** Copy input tensor into the given host buffer. Function to be specialized by
    *  the architecture-specific backend. */
   void CopyTensorInput(HostBuffer_t &buffer, IndexIterator_t begin);
   /** Copy output matrix into the given host buffer. Function to be specialized
    * by the architecture-spcific backend. */
   void CopyTensorOutput(HostBuffer_t &buffer, IndexIterator_t begin);
   /** Copy weight matrix into the given host buffer. Function to be specialized
    * by the architecture-spcific backend. */
   void CopyTensorWeights(HostBuffer_t &buffer, IndexIterator_t begin);

   BatchIterator_t begin() { return TTensorBatchIterator<Data_t, Architecture_t>(*this); }
   BatchIterator_t end() { return TTensorBatchIterator<Data_t, Architecture_t>(*this, fNSamples / fBatchSize); }

   /** Shuffle the order of the samples in the batch. The shuffling is indirect,
    *  i.e. only the indices are shuffled. No input data is moved by this
    * routine. */
   void Shuffle();

   /** Return the next batch from the training set. The TTensorDataLoader object
    *  keeps an internal counter that cycles over the batches in the training
    *  set. */
   TTensorBatch<Architecture_t> GetTensorBatch();
};

//
// TTensorBatch Class.
//______________________________________________________________________________
template <typename Architecture_t>
TTensorBatch<Architecture_t>::TTensorBatch(std::vector<Matrix_t> &inputTensor, Matrix_t &outputMatrix,
                                           Matrix_t &weightMatrix)
   : fInputTensor(inputTensor), fOutputMatrix(outputMatrix), fWeightMatrix(weightMatrix)
{
   // Nothing to do here.
}

//
// TTensorDataLoader Class.
//______________________________________________________________________________
template <typename Data_t, typename Architecture_t>
TTensorDataLoader<Data_t, Architecture_t>::TTensorDataLoader(const Data_t &data, size_t nSamples, size_t batchSize,
                                                             size_t batchDepth, size_t batchHeight, size_t batchWidth,
                                                             size_t nOutputFeatures, size_t nStreams)
   : fData(data), fNSamples(nSamples), fBatchSize(batchSize), fBatchDepth(batchDepth), fBatchHeight(batchHeight),
     fBatchWidth(batchWidth), fNOutputFeatures(nOutputFeatures), fBatchIndex(0), fNStreams(nStreams), fDeviceBuffers(),
     fHostBuffers(), fSampleIndices()
{
   size_t inputTensorSize = fBatchDepth * fBatchHeight * fBatchWidth;
   size_t outputMatrixSize = fBatchSize * fNOutputFeatures;
   size_t weightMatrixSize = fBatchSize;

   for (size_t i = 0; i < fNStreams; i++) {
      fHostBuffers.push_back(HostBuffer_t(inputTensorSize + outputMatrixSize + weightMatrixSize));
      fDeviceBuffers.push_back(DeviceBuffer_t(inputTensorSize + outputMatrixSize + weightMatrixSize));
   }

   fSampleIndices.reserve(fNSamples);
   for (size_t i = 0; i < fNSamples; i++) {
      fSampleIndices.push_back(i);
   }
}

//______________________________________________________________________________
template <typename Data_t, typename Architecture_t>
TTensorBatch<Architecture_t> TTensorDataLoader<Data_t, Architecture_t>::GetTensorBatch()
{
   fBatchIndex %= (fNSamples / fBatchSize); // Cycle through samples.

   size_t inputTensorSize = fBatchDepth * fBatchHeight * fBatchWidth;
   size_t outputMatrixSize = fBatchSize * fNOutputFeatures;
   size_t weightMatrixSize = fBatchSize;

   size_t streamIndex = fBatchIndex % fNStreams;
   HostBuffer_t &hostBuffer = fHostBuffers[streamIndex];
   DeviceBuffer_t &deviceBuffer = fDeviceBuffers[streamIndex];

   HostBuffer_t inputHostBuffer = hostBuffer.GetSubBuffer(0, inputTensorSize);
   HostBuffer_t outputHostBuffer = hostBuffer.GetSubBuffer(inputTensorSize, outputMatrixSize);
   HostBuffer_t weightHostBuffer = hostBuffer.GetSubBuffer(inputTensorSize + outputMatrixSize, weightMatrixSize);

   DeviceBuffer_t inputDeviceBuffer = deviceBuffer.GetSubBuffer(0, inputTensorSize);
   DeviceBuffer_t outputDeviceBuffer = deviceBuffer.GetSubBuffer(inputTensorSize, outputMatrixSize);
   DeviceBuffer_t weightDeviceBuffer = deviceBuffer.GetSubBuffer(inputTensorSize + outputMatrixSize, weightMatrixSize);

   size_t sampleIndex = fBatchIndex * fBatchSize;
   IndexIterator_t sampleIndexIterator = fSampleIndices.begin() + sampleIndex;

   CopyTensorInput(inputHostBuffer, sampleIndexIterator);
   CopyTensorOutput(outputHostBuffer, sampleIndexIterator);
   CopyTensorWeights(weightHostBuffer, sampleIndexIterator);

   deviceBuffer.CopyFrom(hostBuffer);

   std::vector<Matrix_t> inputTensor;
   size_t jump = fBatchHeight * fBatchWidth;
   for (size_t i = 0; i < fBatchDepth; i++) {
      DeviceBuffer_t subInputDeviceBuffer = inputDeviceBuffer.GetSubBuffer(i * jump, jump);
      inputTensor.emplace_back(subInputDeviceBuffer, fBatchHeight, fBatchWidth);
   }
   Matrix_t outputMatrix(outputDeviceBuffer, fBatchSize, fNOutputFeatures);
   Matrix_t weightMatrix(weightDeviceBuffer, fBatchSize, fNOutputFeatures);

   fBatchIndex++;
   return TTensorBatch<Architecture_t>(inputTensor, outputMatrix, weightMatrix);
}

//______________________________________________________________________________
template <typename Data_t, typename Architecture_t>
void TTensorDataLoader<Data_t, Architecture_t>::Shuffle()
{
   std::random_shuffle(fSampleIndices.begin(), fSampleIndices.end());
}

} // namespace DNN
} // namespace TMVA

#endif