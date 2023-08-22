#include "rocblaslt.h"
#include "rocblaslt-types.h"
#include "handle.h"
#include "kernels/matrix_transform.hpp"
#include <hipblaslt/hipblaslt-types.h>
#include <functional>
#include <tuple>
#include <map>

namespace {
    template<typename DType, typename ScaleType,
    bool RowMajA, bool RowMajB, bool RowMajC,
    uint32_t NumThreadsM, uint32_t NumThreadsN, uint32_t VectorWidth>
hipError_t launchTransformKernel(
    DType *c, const DType *a, const DType *b,
    ScaleType alpha, ScaleType beta,
    uint32_t m, uint32_t n,
    uint32_t ldA, uint32_t ldB, uint32_t ldC,
    uint32_t batchSize, uint32_t batchStride,
    bool transA, bool transB,
    hipStream_t stream) {
    constexpr auto TileM = RowMajC ? NumThreadsM : NumThreadsM * VectorWidth;
    constexpr auto TileN = RowMajC ? NumThreadsN * VectorWidth : NumThreadsN;
    const auto numWg = (m / TileM + !!(m % TileM)) * (n / TileN + !!(n % TileN));
    constexpr auto numWorkitems = NumThreadsM * NumThreadsN;
    amd_detail::transform<DType, ScaleType,
        RowMajA, RowMajB, RowMajC, NumThreadsM, NumThreadsN, VectorWidth><<<dim3{numWg, 1, batchSize}, numWorkitems, 0, stream>>>(c, a, b, alpha, beta, m, n, ldA, ldB, ldC, batchStride, transA, transB);

    return hipSuccess;
}
    /*                                          input dtype          scale type           order A           order B           order C*/
    using MatrixTransformKernelKey = std::tuple<hipblasltDatatype_t, hipblasltDatatype_t, hipblasLtOrder_t, hipblasLtOrder_t, hipblasLtOrder_t>;
    /*                                                 C       A             B             alpha         beta          m         n         ld A      ld B      ld C      batchSize batch stride*/
    using MatrixTransformFunction = std::function<void(void *, const void *, const void *, const void *, const void *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, bool, bool, hipStream_t)>;
    std::map<MatrixTransformKernelKey, MatrixTransformFunction> transformKernels{
        {std::make_tuple(HIPBLASLT_R_32F, HIPBLASLT_R_32F, HIPBLASLT_ORDER_COL, HIPBLASLT_ORDER_COL, HIPBLASLT_ORDER_COL), 
        [](void *c, const void *a, const void *b, const void *alpha, const void *beta, uint32_t m, uint32_t n, uint32_t ldA, uint32_t ldB, uint32_t ldC, uint32_t batchSize, uint32_t batchStride, bool opA, bool opB, hipStream_t stream) {
            return launchTransformKernel<hipblasLtFloat, hipblasLtFloat, false, false, false, 16, 16, 4>(
        static_cast<hipblasLtFloat *>(c), static_cast<const hipblasLtFloat *>(a), static_cast<const hipblasLtFloat *>(b), *reinterpret_cast<const hipblasLtFloat *>(alpha), *reinterpret_cast<const hipblasLtFloat *>(beta), m, n, ldA, ldB, ldC, batchSize, batchStride, opA, opB, stream);
        }},
        {std::make_tuple(HIPBLASLT_R_32F, HIPBLASLT_R_32F, HIPBLASLT_ORDER_COL, HIPBLASLT_ORDER_COL, HIPBLASLT_ORDER_ROW), 
        [](void *c, const void *a, const void *b, const void *alpha, const void *beta, uint32_t m, uint32_t n, uint32_t ldA, uint32_t ldB, uint32_t ldC, uint32_t batchSize, uint32_t batchStride, bool opA, bool opB, hipStream_t stream) {
            return launchTransformKernel<hipblasLtFloat, hipblasLtFloat, false, false, true, 16, 16, 4>(
        static_cast<hipblasLtFloat *>(c), static_cast<const hipblasLtFloat *>(a), static_cast<const hipblasLtFloat *>(b), *reinterpret_cast<const hipblasLtFloat *>(alpha), *reinterpret_cast<const hipblasLtFloat *>(beta), m, n, ldA, ldB, ldC, batchSize, batchStride, opA, opB, stream);
        }},
        {std::make_tuple(HIPBLASLT_R_32F, HIPBLASLT_R_32F, HIPBLASLT_ORDER_COL, HIPBLASLT_ORDER_ROW, HIPBLASLT_ORDER_COL), 
        [](void *c, const void *a, const void *b, const void *alpha, const void *beta, uint32_t m, uint32_t n, uint32_t ldA, uint32_t ldB, uint32_t ldC, uint32_t batchSize, uint32_t batchStride, bool opA, bool opB, hipStream_t stream) {
            return launchTransformKernel<hipblasLtFloat, hipblasLtFloat, false, true, false, 16, 16, 4>(
        static_cast<hipblasLtFloat *>(c), static_cast<const hipblasLtFloat *>(a), static_cast<const hipblasLtFloat *>(b), *reinterpret_cast<const hipblasLtFloat *>(alpha), *reinterpret_cast<const hipblasLtFloat *>(beta), m, n, ldA, ldB, ldC, batchSize, batchStride, opA, opB, stream);
        }},
        {std::make_tuple(HIPBLASLT_R_32F, HIPBLASLT_R_32F, HIPBLASLT_ORDER_COL, HIPBLASLT_ORDER_ROW, HIPBLASLT_ORDER_ROW), 
        [](void *c, const void *a, const void *b, const void *alpha, const void *beta, uint32_t m, uint32_t n, uint32_t ldA, uint32_t ldB, uint32_t ldC, uint32_t batchSize, uint32_t batchStride, bool opA, bool opB, hipStream_t stream) {
            return launchTransformKernel<hipblasLtFloat, hipblasLtFloat, false, true, true, 16, 16, 4>(
        static_cast<hipblasLtFloat *>(c), static_cast<const hipblasLtFloat *>(a), static_cast<const hipblasLtFloat *>(b), *reinterpret_cast<const hipblasLtFloat *>(alpha), *reinterpret_cast<const hipblasLtFloat *>(beta), m, n, ldA, ldB, ldC, batchSize, batchStride, opA, opB, stream);
        }},
        {std::make_tuple(HIPBLASLT_R_32F, HIPBLASLT_R_32F, HIPBLASLT_ORDER_ROW, HIPBLASLT_ORDER_COL, HIPBLASLT_ORDER_COL), 
        [](void *c, const void *a, const void *b, const void *alpha, const void *beta, uint32_t m, uint32_t n, uint32_t ldA, uint32_t ldB, uint32_t ldC, uint32_t batchSize, uint32_t batchStride, bool opA, bool opB, hipStream_t stream) {
            return launchTransformKernel<hipblasLtFloat, hipblasLtFloat, true, false, false, 16, 16, 4>(
        static_cast<hipblasLtFloat *>(c), static_cast<const hipblasLtFloat *>(a), static_cast<const hipblasLtFloat *>(b), *reinterpret_cast<const hipblasLtFloat *>(alpha), *reinterpret_cast<const hipblasLtFloat *>(beta), m, n, ldA, ldB, ldC, batchSize, batchStride, opA, opB, stream);
        }},
        {std::make_tuple(HIPBLASLT_R_32F, HIPBLASLT_R_32F, HIPBLASLT_ORDER_ROW, HIPBLASLT_ORDER_COL, HIPBLASLT_ORDER_ROW), 
        [](void *c, const void *a, const void *b, const void *alpha, const void *beta, uint32_t m, uint32_t n, uint32_t ldA, uint32_t ldB, uint32_t ldC, uint32_t batchSize, uint32_t batchStride, bool opA, bool opB, hipStream_t stream) {
            return launchTransformKernel<hipblasLtFloat, hipblasLtFloat, true, false, true, 16, 16, 4>(
        static_cast<hipblasLtFloat *>(c), static_cast<const hipblasLtFloat *>(a), static_cast<const hipblasLtFloat *>(b), *reinterpret_cast<const hipblasLtFloat *>(alpha), *reinterpret_cast<const hipblasLtFloat *>(beta), m, n, ldA, ldB, ldC, batchSize, batchStride, opA, opB, stream);
        }},
        {std::make_tuple(HIPBLASLT_R_32F, HIPBLASLT_R_32F, HIPBLASLT_ORDER_ROW, HIPBLASLT_ORDER_ROW, HIPBLASLT_ORDER_COL), 
        [](void *c, const void *a, const void *b, const void *alpha, const void *beta, uint32_t m, uint32_t n, uint32_t ldA, uint32_t ldB, uint32_t ldC, uint32_t batchSize, uint32_t batchStride, bool opA, bool opB, hipStream_t stream) {
            return launchTransformKernel<hipblasLtFloat, hipblasLtFloat, true, true, false, 16, 16, 4>(
        static_cast<hipblasLtFloat *>(c), static_cast<const hipblasLtFloat *>(a), static_cast<const hipblasLtFloat *>(b), *reinterpret_cast<const hipblasLtFloat *>(alpha), *reinterpret_cast<const hipblasLtFloat *>(beta), m, n, ldA, ldB, ldC, batchSize, batchStride, opA, opB, stream);
        }},
        {std::make_tuple(HIPBLASLT_R_32F, HIPBLASLT_R_32F, HIPBLASLT_ORDER_ROW, HIPBLASLT_ORDER_ROW, HIPBLASLT_ORDER_ROW), 
        [](void *c, const void *a, const void *b, const void *alpha, const void *beta, uint32_t m, uint32_t n, uint32_t ldA, uint32_t ldB, uint32_t ldC, uint32_t batchSize, uint32_t batchStride, bool opA, bool opB, hipStream_t stream) {
            return launchTransformKernel<hipblasLtFloat, hipblasLtFloat, true, true, true, 16, 16, 4>(
        static_cast<hipblasLtFloat *>(c), static_cast<const hipblasLtFloat *>(a), static_cast<const hipblasLtFloat *>(b), *reinterpret_cast<const hipblasLtFloat *>(alpha), *reinterpret_cast<const hipblasLtFloat *>(beta), m, n, ldA, ldB, ldC, batchSize, batchStride, opA, opB, stream);
        }},
        {std::make_tuple(HIPBLASLT_R_16F, HIPBLASLT_R_16F, HIPBLASLT_ORDER_COL, HIPBLASLT_ORDER_COL, HIPBLASLT_ORDER_COL), 
        [](void *c, const void *a, const void *b, const void *alpha, const void *beta, uint32_t m, uint32_t n, uint32_t ldA, uint32_t ldB, uint32_t ldC, uint32_t batchSize, uint32_t batchStride, bool opA, bool opB, hipStream_t stream) {
            return launchTransformKernel<hipblasLtHalf, hipblasLtHalf, false, false, false, 16, 16, 4>(
        static_cast<hipblasLtHalf *>(c), static_cast<const hipblasLtHalf *>(a), static_cast<const hipblasLtHalf *>(b), *reinterpret_cast<const hipblasLtHalf *>(alpha), *reinterpret_cast<const hipblasLtHalf *>(beta), m, n, ldA, ldB, ldC, batchSize, batchStride, opA, opB, stream);
        }},
        {std::make_tuple(HIPBLASLT_R_16F, HIPBLASLT_R_16F, HIPBLASLT_ORDER_COL, HIPBLASLT_ORDER_COL, HIPBLASLT_ORDER_ROW), 
        [](void *c, const void *a, const void *b, const void *alpha, const void *beta, uint32_t m, uint32_t n, uint32_t ldA, uint32_t ldB, uint32_t ldC, uint32_t batchSize, uint32_t batchStride, bool opA, bool opB, hipStream_t stream) {
            return launchTransformKernel<hipblasLtHalf, hipblasLtHalf, false, false, true, 16, 16, 4>(
        static_cast<hipblasLtHalf *>(c), static_cast<const hipblasLtHalf *>(a), static_cast<const hipblasLtHalf *>(b), *reinterpret_cast<const hipblasLtHalf *>(alpha), *reinterpret_cast<const hipblasLtHalf *>(beta), m, n, ldA, ldB, ldC, batchSize, batchStride, opA, opB, stream);
        }},
        {std::make_tuple(HIPBLASLT_R_16F, HIPBLASLT_R_16F, HIPBLASLT_ORDER_COL, HIPBLASLT_ORDER_ROW, HIPBLASLT_ORDER_COL), 
        [](void *c, const void *a, const void *b, const void *alpha, const void *beta, uint32_t m, uint32_t n, uint32_t ldA, uint32_t ldB, uint32_t ldC, uint32_t batchSize, uint32_t batchStride, bool opA, bool opB, hipStream_t stream) {
            return launchTransformKernel<hipblasLtHalf, hipblasLtHalf, false, true, false, 16, 16, 4>(
        static_cast<hipblasLtHalf *>(c), static_cast<const hipblasLtHalf *>(a), static_cast<const hipblasLtHalf *>(b), *reinterpret_cast<const hipblasLtHalf *>(alpha), *reinterpret_cast<const hipblasLtHalf *>(beta), m, n, ldA, ldB, ldC, batchSize, batchStride, opA, opB, stream);
        }},
        {std::make_tuple(HIPBLASLT_R_16F, HIPBLASLT_R_16F, HIPBLASLT_ORDER_COL, HIPBLASLT_ORDER_ROW, HIPBLASLT_ORDER_ROW), 
        [](void *c, const void *a, const void *b, const void *alpha, const void *beta, uint32_t m, uint32_t n, uint32_t ldA, uint32_t ldB, uint32_t ldC, uint32_t batchSize, uint32_t batchStride, bool opA, bool opB, hipStream_t stream) {
            return launchTransformKernel<hipblasLtHalf, hipblasLtHalf, false, true, true, 16, 16, 4>(
        static_cast<hipblasLtHalf *>(c), static_cast<const hipblasLtHalf *>(a), static_cast<const hipblasLtHalf *>(b), *reinterpret_cast<const hipblasLtHalf *>(alpha), *reinterpret_cast<const hipblasLtHalf *>(beta), m, n, ldA, ldB, ldC, batchSize, batchStride, opA, opB, stream);
        }},
        {std::make_tuple(HIPBLASLT_R_16F, HIPBLASLT_R_16F, HIPBLASLT_ORDER_ROW, HIPBLASLT_ORDER_COL, HIPBLASLT_ORDER_COL), 
        [](void *c, const void *a, const void *b, const void *alpha, const void *beta, uint32_t m, uint32_t n, uint32_t ldA, uint32_t ldB, uint32_t ldC, uint32_t batchSize, uint32_t batchStride, bool opA, bool opB, hipStream_t stream) {
            return launchTransformKernel<hipblasLtHalf, hipblasLtHalf, true, false, false, 16, 16, 4>(
        static_cast<hipblasLtHalf *>(c), static_cast<const hipblasLtHalf *>(a), static_cast<const hipblasLtHalf *>(b), *reinterpret_cast<const hipblasLtHalf *>(alpha), *reinterpret_cast<const hipblasLtHalf *>(beta), m, n, ldA, ldB, ldC, batchSize, batchStride, opA, opB, stream);
        }},
        {std::make_tuple(HIPBLASLT_R_16F, HIPBLASLT_R_16F, HIPBLASLT_ORDER_ROW, HIPBLASLT_ORDER_COL, HIPBLASLT_ORDER_ROW), 
        [](void *c, const void *a, const void *b, const void *alpha, const void *beta, uint32_t m, uint32_t n, uint32_t ldA, uint32_t ldB, uint32_t ldC, uint32_t batchSize, uint32_t batchStride, bool opA, bool opB, hipStream_t stream) {
            return launchTransformKernel<hipblasLtHalf, hipblasLtHalf, true, false, true, 16, 16, 4>(
        static_cast<hipblasLtHalf *>(c), static_cast<const hipblasLtHalf *>(a), static_cast<const hipblasLtHalf *>(b), *reinterpret_cast<const hipblasLtHalf *>(alpha), *reinterpret_cast<const hipblasLtHalf *>(beta), m, n, ldA, ldB, ldC, batchSize, batchStride, opA, opB, stream);
        }},
        {std::make_tuple(HIPBLASLT_R_16F, HIPBLASLT_R_16F, HIPBLASLT_ORDER_ROW, HIPBLASLT_ORDER_ROW, HIPBLASLT_ORDER_COL), 
        [](void *c, const void *a, const void *b, const void *alpha, const void *beta, uint32_t m, uint32_t n, uint32_t ldA, uint32_t ldB, uint32_t ldC, uint32_t batchSize, uint32_t batchStride, bool opA, bool opB, hipStream_t stream) {
            return launchTransformKernel<hipblasLtHalf, hipblasLtHalf, true, true, false, 16, 16, 4>(
        static_cast<hipblasLtHalf *>(c), static_cast<const hipblasLtHalf *>(a), static_cast<const hipblasLtHalf *>(b), *reinterpret_cast<const hipblasLtHalf *>(alpha), *reinterpret_cast<const hipblasLtHalf *>(beta), m, n, ldA, ldB, ldC, batchSize, batchStride, opA, opB, stream);
        }},
        {std::make_tuple(HIPBLASLT_R_16F, HIPBLASLT_R_16F, HIPBLASLT_ORDER_ROW, HIPBLASLT_ORDER_ROW, HIPBLASLT_ORDER_ROW), 
        [](void *c, const void *a, const void *b, const void *alpha, const void *beta, uint32_t m, uint32_t n, uint32_t ldA, uint32_t ldB, uint32_t ldC, uint32_t batchSize, uint32_t batchStride, bool opA, bool opB, hipStream_t stream) {
            return launchTransformKernel<hipblasLtHalf, hipblasLtHalf, true, true, true, 16, 16, 4>(
        static_cast<hipblasLtHalf *>(c), static_cast<const hipblasLtHalf *>(a), static_cast<const hipblasLtHalf *>(b), *reinterpret_cast<const hipblasLtHalf *>(alpha), *reinterpret_cast<const hipblasLtHalf *>(beta), m, n, ldA, ldB, ldC, batchSize, batchStride, opA, opB, stream);
        }},
        {std::make_tuple(HIPBLASLT_R_16F, HIPBLASLT_R_32F, HIPBLASLT_ORDER_COL, HIPBLASLT_ORDER_COL, HIPBLASLT_ORDER_COL), 
        [](void *c, const void *a, const void *b, const void *alpha, const void *beta, uint32_t m, uint32_t n, uint32_t ldA, uint32_t ldB, uint32_t ldC, uint32_t batchSize, uint32_t batchStride, bool opA, bool opB, hipStream_t stream) {
            return launchTransformKernel<hipblasLtHalf, hipblasLtFloat, false, false, false, 16, 16, 4>(
        static_cast<hipblasLtHalf *>(c), static_cast<const hipblasLtHalf *>(a), static_cast<const hipblasLtHalf *>(b), *reinterpret_cast<const hipblasLtFloat *>(alpha), *reinterpret_cast<const hipblasLtFloat *>(beta), m, n, ldA, ldB, ldC, batchSize, batchStride, opA, opB, stream);
        }},
        {std::make_tuple(HIPBLASLT_R_16F, HIPBLASLT_R_32F, HIPBLASLT_ORDER_COL, HIPBLASLT_ORDER_COL, HIPBLASLT_ORDER_ROW), 
        [](void *c, const void *a, const void *b, const void *alpha, const void *beta, uint32_t m, uint32_t n, uint32_t ldA, uint32_t ldB, uint32_t ldC, uint32_t batchSize, uint32_t batchStride, bool opA, bool opB, hipStream_t stream) {
            return launchTransformKernel<hipblasLtHalf, hipblasLtFloat, false, false, true, 16, 16, 4>(
        static_cast<hipblasLtHalf *>(c), static_cast<const hipblasLtHalf *>(a), static_cast<const hipblasLtHalf *>(b), *reinterpret_cast<const hipblasLtFloat *>(alpha), *reinterpret_cast<const hipblasLtFloat *>(beta), m, n, ldA, ldB, ldC, batchSize, batchStride, opA, opB, stream);
        }},
        {std::make_tuple(HIPBLASLT_R_16F, HIPBLASLT_R_32F, HIPBLASLT_ORDER_COL, HIPBLASLT_ORDER_ROW, HIPBLASLT_ORDER_COL), 
        [](void *c, const void *a, const void *b, const void *alpha, const void *beta, uint32_t m, uint32_t n, uint32_t ldA, uint32_t ldB, uint32_t ldC, uint32_t batchSize, uint32_t batchStride, bool opA, bool opB, hipStream_t stream) {
            return launchTransformKernel<hipblasLtHalf, hipblasLtFloat, false, true, false, 16, 16, 4>(
        static_cast<hipblasLtHalf *>(c), static_cast<const hipblasLtHalf *>(a), static_cast<const hipblasLtHalf *>(b), *reinterpret_cast<const hipblasLtFloat *>(alpha), *reinterpret_cast<const hipblasLtFloat *>(beta), m, n, ldA, ldB, ldC, batchSize, batchStride, opA, opB, stream);
        }},
        {std::make_tuple(HIPBLASLT_R_16F, HIPBLASLT_R_32F, HIPBLASLT_ORDER_COL, HIPBLASLT_ORDER_ROW, HIPBLASLT_ORDER_ROW), 
        [](void *c, const void *a, const void *b, const void *alpha, const void *beta, uint32_t m, uint32_t n, uint32_t ldA, uint32_t ldB, uint32_t ldC, uint32_t batchSize, uint32_t batchStride, bool opA, bool opB, hipStream_t stream) {
            return launchTransformKernel<hipblasLtHalf, hipblasLtFloat, false, true, true, 16, 16, 4>(
        static_cast<hipblasLtHalf *>(c), static_cast<const hipblasLtHalf *>(a), static_cast<const hipblasLtHalf *>(b), *reinterpret_cast<const hipblasLtFloat *>(alpha), *reinterpret_cast<const hipblasLtFloat *>(beta), m, n, ldA, ldB, ldC, batchSize, batchStride, opA, opB, stream);
        }},
        {std::make_tuple(HIPBLASLT_R_16F, HIPBLASLT_R_32F, HIPBLASLT_ORDER_ROW, HIPBLASLT_ORDER_COL, HIPBLASLT_ORDER_COL), 
        [](void *c, const void *a, const void *b, const void *alpha, const void *beta, uint32_t m, uint32_t n, uint32_t ldA, uint32_t ldB, uint32_t ldC, uint32_t batchSize, uint32_t batchStride, bool opA, bool opB, hipStream_t stream) {
            return launchTransformKernel<hipblasLtHalf, hipblasLtFloat, true, false, false, 16, 16, 4>(
        static_cast<hipblasLtHalf *>(c), static_cast<const hipblasLtHalf *>(a), static_cast<const hipblasLtHalf *>(b), *reinterpret_cast<const hipblasLtFloat *>(alpha), *reinterpret_cast<const hipblasLtFloat *>(beta), m, n, ldA, ldB, ldC, batchSize, batchStride, opA, opB, stream);
        }},
        {std::make_tuple(HIPBLASLT_R_16F, HIPBLASLT_R_32F, HIPBLASLT_ORDER_ROW, HIPBLASLT_ORDER_COL, HIPBLASLT_ORDER_ROW), 
        [](void *c, const void *a, const void *b, const void *alpha, const void *beta, uint32_t m, uint32_t n, uint32_t ldA, uint32_t ldB, uint32_t ldC, uint32_t batchSize, uint32_t batchStride, bool opA, bool opB, hipStream_t stream) {
            return launchTransformKernel<hipblasLtHalf, hipblasLtFloat, true, false, true, 16, 16, 4>(
        static_cast<hipblasLtHalf *>(c), static_cast<const hipblasLtHalf *>(a), static_cast<const hipblasLtHalf *>(b), *reinterpret_cast<const hipblasLtFloat *>(alpha), *reinterpret_cast<const hipblasLtFloat *>(beta), m, n, ldA, ldB, ldC, batchSize, batchStride, opA, opB, stream);
        }},
        {std::make_tuple(HIPBLASLT_R_16F, HIPBLASLT_R_32F, HIPBLASLT_ORDER_ROW, HIPBLASLT_ORDER_ROW, HIPBLASLT_ORDER_COL), 
        [](void *c, const void *a, const void *b, const void *alpha, const void *beta, uint32_t m, uint32_t n, uint32_t ldA, uint32_t ldB, uint32_t ldC, uint32_t batchSize, uint32_t batchStride, bool opA, bool opB, hipStream_t stream) {
            return launchTransformKernel<hipblasLtHalf, hipblasLtFloat, true, true, false, 16, 16, 4>(
        static_cast<hipblasLtHalf *>(c), static_cast<const hipblasLtHalf *>(a), static_cast<const hipblasLtHalf *>(b), *reinterpret_cast<const hipblasLtFloat *>(alpha), *reinterpret_cast<const hipblasLtFloat *>(beta), m, n, ldA, ldB, ldC, batchSize, batchStride, opA, opB, stream);
        }},
        {std::make_tuple(HIPBLASLT_R_16F, HIPBLASLT_R_32F, HIPBLASLT_ORDER_ROW, HIPBLASLT_ORDER_ROW, HIPBLASLT_ORDER_ROW), 
        [](void *c, const void *a, const void *b, const void *alpha, const void *beta, uint32_t m, uint32_t n, uint32_t ldA, uint32_t ldB, uint32_t ldC, uint32_t batchSize, uint32_t batchStride, bool opA, bool opB, hipStream_t stream) {
            return launchTransformKernel<hipblasLtHalf, hipblasLtFloat, true, true, true, 16, 16, 4>(
        static_cast<hipblasLtHalf *>(c), static_cast<const hipblasLtHalf *>(a), static_cast<const hipblasLtHalf *>(b), *reinterpret_cast<const hipblasLtFloat *>(alpha), *reinterpret_cast<const hipblasLtFloat *>(beta), m, n, ldA, ldB, ldC, batchSize, batchStride, opA, opB, stream);
        }},
        {std::make_tuple(HIPBLASLT_R_16B, HIPBLASLT_R_32F, HIPBLASLT_ORDER_COL, HIPBLASLT_ORDER_COL, HIPBLASLT_ORDER_COL), 
        [](void *c, const void *a, const void *b, const void *alpha, const void *beta, uint32_t m, uint32_t n, uint32_t ldA, uint32_t ldB, uint32_t ldC, uint32_t batchSize, uint32_t batchStride, bool opA, bool opB, hipStream_t stream) {
            return launchTransformKernel<hipblasLtBfloat16, hipblasLtFloat, false, false, false, 16, 16, 4>(
        static_cast<hipblasLtBfloat16 *>(c), static_cast<const hipblasLtBfloat16 *>(a), static_cast<const hipblasLtBfloat16 *>(b), *reinterpret_cast<const hipblasLtFloat *>(alpha), *reinterpret_cast<const hipblasLtFloat *>(beta), m, n, ldA, ldB, ldC, batchSize, batchStride, opA, opB, stream);
        }},
        {std::make_tuple(HIPBLASLT_R_16B, HIPBLASLT_R_32F, HIPBLASLT_ORDER_COL, HIPBLASLT_ORDER_COL, HIPBLASLT_ORDER_ROW), 
        [](void *c, const void *a, const void *b, const void *alpha, const void *beta, uint32_t m, uint32_t n, uint32_t ldA, uint32_t ldB, uint32_t ldC, uint32_t batchSize, uint32_t batchStride, bool opA, bool opB, hipStream_t stream) {
            return launchTransformKernel<hipblasLtBfloat16, hipblasLtFloat, false, false, true, 16, 16, 4>(
        static_cast<hipblasLtBfloat16 *>(c), static_cast<const hipblasLtBfloat16 *>(a), static_cast<const hipblasLtBfloat16 *>(b), *reinterpret_cast<const hipblasLtFloat *>(alpha), *reinterpret_cast<const hipblasLtFloat *>(beta), m, n, ldA, ldB, ldC, batchSize, batchStride, opA, opB, stream);
        }},
        {std::make_tuple(HIPBLASLT_R_16B, HIPBLASLT_R_32F, HIPBLASLT_ORDER_COL, HIPBLASLT_ORDER_ROW, HIPBLASLT_ORDER_COL), 
        [](void *c, const void *a, const void *b, const void *alpha, const void *beta, uint32_t m, uint32_t n, uint32_t ldA, uint32_t ldB, uint32_t ldC, uint32_t batchSize, uint32_t batchStride, bool opA, bool opB, hipStream_t stream) {
            return launchTransformKernel<hipblasLtBfloat16, hipblasLtFloat, false, true, false, 16, 16, 4>(
        static_cast<hipblasLtBfloat16 *>(c), static_cast<const hipblasLtBfloat16 *>(a), static_cast<const hipblasLtBfloat16 *>(b), *reinterpret_cast<const hipblasLtFloat *>(alpha), *reinterpret_cast<const hipblasLtFloat *>(beta), m, n, ldA, ldB, ldC, batchSize, batchStride, opA, opB, stream);
        }},
        {std::make_tuple(HIPBLASLT_R_16B, HIPBLASLT_R_32F, HIPBLASLT_ORDER_COL, HIPBLASLT_ORDER_ROW, HIPBLASLT_ORDER_ROW), 
        [](void *c, const void *a, const void *b, const void *alpha, const void *beta, uint32_t m, uint32_t n, uint32_t ldA, uint32_t ldB, uint32_t ldC, uint32_t batchSize, uint32_t batchStride, bool opA, bool opB, hipStream_t stream) {
            return launchTransformKernel<hipblasLtBfloat16, hipblasLtFloat, false, true, true, 16, 16, 4>(
        static_cast<hipblasLtBfloat16 *>(c), static_cast<const hipblasLtBfloat16 *>(a), static_cast<const hipblasLtBfloat16 *>(b), *reinterpret_cast<const hipblasLtFloat *>(alpha), *reinterpret_cast<const hipblasLtFloat *>(beta), m, n, ldA, ldB, ldC, batchSize, batchStride, opA, opB, stream);
        }},
        {std::make_tuple(HIPBLASLT_R_16B, HIPBLASLT_R_32F, HIPBLASLT_ORDER_ROW, HIPBLASLT_ORDER_COL, HIPBLASLT_ORDER_COL), 
        [](void *c, const void *a, const void *b, const void *alpha, const void *beta, uint32_t m, uint32_t n, uint32_t ldA, uint32_t ldB, uint32_t ldC, uint32_t batchSize, uint32_t batchStride, bool opA, bool opB, hipStream_t stream) {
            return launchTransformKernel<hipblasLtBfloat16, hipblasLtFloat, true, false, false, 16, 16, 4>(
        static_cast<hipblasLtBfloat16 *>(c), static_cast<const hipblasLtBfloat16 *>(a), static_cast<const hipblasLtBfloat16 *>(b), *reinterpret_cast<const hipblasLtFloat *>(alpha), *reinterpret_cast<const hipblasLtFloat *>(beta), m, n, ldA, ldB, ldC, batchSize, batchStride, opA, opB, stream);
        }},
        {std::make_tuple(HIPBLASLT_R_16B, HIPBLASLT_R_32F, HIPBLASLT_ORDER_ROW, HIPBLASLT_ORDER_COL, HIPBLASLT_ORDER_ROW), 
        [](void *c, const void *a, const void *b, const void *alpha, const void *beta, uint32_t m, uint32_t n, uint32_t ldA, uint32_t ldB, uint32_t ldC, uint32_t batchSize, uint32_t batchStride, bool opA, bool opB, hipStream_t stream) {
            return launchTransformKernel<hipblasLtBfloat16, hipblasLtFloat, true, false, true, 16, 16, 4>(
        static_cast<hipblasLtBfloat16 *>(c), static_cast<const hipblasLtBfloat16 *>(a), static_cast<const hipblasLtBfloat16 *>(b), *reinterpret_cast<const hipblasLtFloat *>(alpha), *reinterpret_cast<const hipblasLtFloat *>(beta), m, n, ldA, ldB, ldC, batchSize, batchStride, opA, opB, stream);
        }},
        {std::make_tuple(HIPBLASLT_R_16B, HIPBLASLT_R_32F, HIPBLASLT_ORDER_ROW, HIPBLASLT_ORDER_ROW, HIPBLASLT_ORDER_COL), 
        [](void *c, const void *a, const void *b, const void *alpha, const void *beta, uint32_t m, uint32_t n, uint32_t ldA, uint32_t ldB, uint32_t ldC, uint32_t batchSize, uint32_t batchStride, bool opA, bool opB, hipStream_t stream) {
            return launchTransformKernel<hipblasLtBfloat16, hipblasLtFloat, true, true, false, 16, 16, 4>(
        static_cast<hipblasLtBfloat16 *>(c), static_cast<const hipblasLtBfloat16 *>(a), static_cast<const hipblasLtBfloat16 *>(b), *reinterpret_cast<const hipblasLtFloat *>(alpha), *reinterpret_cast<const hipblasLtFloat *>(beta), m, n, ldA, ldB, ldC, batchSize, batchStride, opA, opB, stream);
        }},
        {std::make_tuple(HIPBLASLT_R_16B, HIPBLASLT_R_32F, HIPBLASLT_ORDER_ROW, HIPBLASLT_ORDER_ROW, HIPBLASLT_ORDER_ROW), 
        [](void *c, const void *a, const void *b, const void *alpha, const void *beta, uint32_t m, uint32_t n, uint32_t ldA, uint32_t ldB, uint32_t ldC, uint32_t batchSize, uint32_t batchStride, bool opA, bool opB, hipStream_t stream) {
            return launchTransformKernel<hipblasLtBfloat16, hipblasLtFloat, true, true, true, 16, 16, 4>(
        static_cast<hipblasLtBfloat16 *>(c), static_cast<const hipblasLtBfloat16 *>(a), static_cast<const hipblasLtBfloat16 *>(b), *reinterpret_cast<const hipblasLtFloat *>(alpha), *reinterpret_cast<const hipblasLtFloat *>(beta), m, n, ldA, ldB, ldC, batchSize, batchStride, opA, opB, stream);
        }},
        {std::make_tuple(HIPBLASLT_R_8I, HIPBLASLT_R_32F, HIPBLASLT_ORDER_COL, HIPBLASLT_ORDER_COL, HIPBLASLT_ORDER_COL), 
        [](void *c, const void *a, const void *b, const void *alpha, const void *beta, uint32_t m, uint32_t n, uint32_t ldA, uint32_t ldB, uint32_t ldC, uint32_t batchSize, uint32_t batchStride, bool opA, bool opB, hipStream_t stream) {
            return launchTransformKernel<int8_t, hipblasLtFloat, false, false, false, 16, 16, 4>(
        static_cast<int8_t *>(c), static_cast<const int8_t *>(a), static_cast<const int8_t *>(b), *reinterpret_cast<const hipblasLtFloat *>(alpha), *reinterpret_cast<const hipblasLtFloat *>(beta), m, n, ldA, ldB, ldC, batchSize, batchStride, opA, opB, stream);
        }},
        {std::make_tuple(HIPBLASLT_R_8I, HIPBLASLT_R_32F, HIPBLASLT_ORDER_COL, HIPBLASLT_ORDER_COL, HIPBLASLT_ORDER_ROW), 
        [](void *c, const void *a, const void *b, const void *alpha, const void *beta, uint32_t m, uint32_t n, uint32_t ldA, uint32_t ldB, uint32_t ldC, uint32_t batchSize, uint32_t batchStride, bool opA, bool opB, hipStream_t stream) {
            return launchTransformKernel<int8_t, hipblasLtFloat, false, false, true, 16, 16, 4>(
        static_cast<int8_t *>(c), static_cast<const int8_t *>(a), static_cast<const int8_t *>(b), *reinterpret_cast<const hipblasLtFloat *>(alpha), *reinterpret_cast<const hipblasLtFloat *>(beta), m, n, ldA, ldB, ldC, batchSize, batchStride, opA, opB, stream);
        }},
        {std::make_tuple(HIPBLASLT_R_8I, HIPBLASLT_R_32F, HIPBLASLT_ORDER_COL, HIPBLASLT_ORDER_ROW, HIPBLASLT_ORDER_COL), 
        [](void *c, const void *a, const void *b, const void *alpha, const void *beta, uint32_t m, uint32_t n, uint32_t ldA, uint32_t ldB, uint32_t ldC, uint32_t batchSize, uint32_t batchStride, bool opA, bool opB, hipStream_t stream) {
            return launchTransformKernel<int8_t, hipblasLtFloat, false, true, false, 16, 16, 4>(
        static_cast<int8_t *>(c), static_cast<const int8_t *>(a), static_cast<const int8_t *>(b), *reinterpret_cast<const hipblasLtFloat *>(alpha), *reinterpret_cast<const hipblasLtFloat *>(beta), m, n, ldA, ldB, ldC, batchSize, batchStride, opA, opB, stream);
        }},
        {std::make_tuple(HIPBLASLT_R_8I, HIPBLASLT_R_32F, HIPBLASLT_ORDER_COL, HIPBLASLT_ORDER_ROW, HIPBLASLT_ORDER_ROW), 
        [](void *c, const void *a, const void *b, const void *alpha, const void *beta, uint32_t m, uint32_t n, uint32_t ldA, uint32_t ldB, uint32_t ldC, uint32_t batchSize, uint32_t batchStride, bool opA, bool opB, hipStream_t stream) {
            return launchTransformKernel<int8_t, hipblasLtFloat, false, true, true, 16, 16, 4>(
        static_cast<int8_t *>(c), static_cast<const int8_t *>(a), static_cast<const int8_t *>(b), *reinterpret_cast<const hipblasLtFloat *>(alpha), *reinterpret_cast<const hipblasLtFloat *>(beta), m, n, ldA, ldB, ldC, batchSize, batchStride, opA, opB, stream);
        }},
        {std::make_tuple(HIPBLASLT_R_8I, HIPBLASLT_R_32F, HIPBLASLT_ORDER_ROW, HIPBLASLT_ORDER_COL, HIPBLASLT_ORDER_COL), 
        [](void *c, const void *a, const void *b, const void *alpha, const void *beta, uint32_t m, uint32_t n, uint32_t ldA, uint32_t ldB, uint32_t ldC, uint32_t batchSize, uint32_t batchStride, bool opA, bool opB, hipStream_t stream) {
            return launchTransformKernel<int8_t, hipblasLtFloat, true, false, false, 16, 16, 4>(
        static_cast<int8_t *>(c), static_cast<const int8_t *>(a), static_cast<const int8_t *>(b), *reinterpret_cast<const hipblasLtFloat *>(alpha), *reinterpret_cast<const hipblasLtFloat *>(beta), m, n, ldA, ldB, ldC, batchSize, batchStride, opA, opB, stream);
        }},
        {std::make_tuple(HIPBLASLT_R_8I, HIPBLASLT_R_32F, HIPBLASLT_ORDER_ROW, HIPBLASLT_ORDER_COL, HIPBLASLT_ORDER_ROW), 
        [](void *c, const void *a, const void *b, const void *alpha, const void *beta, uint32_t m, uint32_t n, uint32_t ldA, uint32_t ldB, uint32_t ldC, uint32_t batchSize, uint32_t batchStride, bool opA, bool opB, hipStream_t stream) {
            return launchTransformKernel<int8_t, hipblasLtFloat, true, false, true, 16, 16, 4>(
        static_cast<int8_t *>(c), static_cast<const int8_t *>(a), static_cast<const int8_t *>(b), *reinterpret_cast<const hipblasLtFloat *>(alpha), *reinterpret_cast<const hipblasLtFloat *>(beta), m, n, ldA, ldB, ldC, batchSize, batchStride, opA, opB, stream);
        }},
        {std::make_tuple(HIPBLASLT_R_8I, HIPBLASLT_R_32F, HIPBLASLT_ORDER_ROW, HIPBLASLT_ORDER_ROW, HIPBLASLT_ORDER_COL), 
        [](void *c, const void *a, const void *b, const void *alpha, const void *beta, uint32_t m, uint32_t n, uint32_t ldA, uint32_t ldB, uint32_t ldC, uint32_t batchSize, uint32_t batchStride, bool opA, bool opB, hipStream_t stream) {
            return launchTransformKernel<int8_t, hipblasLtFloat, true, true, false, 16, 16, 4>(
        static_cast<int8_t *>(c), static_cast<const int8_t *>(a), static_cast<const int8_t *>(b), *reinterpret_cast<const hipblasLtFloat *>(alpha), *reinterpret_cast<const hipblasLtFloat *>(beta), m, n, ldA, ldB, ldC, batchSize, batchStride, opA, opB, stream);
        }},
        {std::make_tuple(HIPBLASLT_R_8I, HIPBLASLT_R_32F, HIPBLASLT_ORDER_ROW, HIPBLASLT_ORDER_ROW, HIPBLASLT_ORDER_ROW), 
        [](void *c, const void *a, const void *b, const void *alpha, const void *beta, uint32_t m, uint32_t n, uint32_t ldA, uint32_t ldB, uint32_t ldC, uint32_t batchSize, uint32_t batchStride, bool opA, bool opB, hipStream_t stream) {
            return launchTransformKernel<int8_t, hipblasLtFloat, true, true, true, 16, 16, 4>(
        static_cast<int8_t *>(c), static_cast<const int8_t *>(a), static_cast<const int8_t *>(b), *reinterpret_cast<const hipblasLtFloat *>(alpha), *reinterpret_cast<const hipblasLtFloat *>(beta), m, n, ldA, ldB, ldC, batchSize, batchStride, opA, opB, stream);
        }}
    };
}

rocblaslt_status rocblaslt_matrix_transform(rocblaslt_handle handle,
                                            rocblaslt_matrix_transform_desc *desc,
                                            const void* alpha, /* host or device pointer */
                                            const void* A,
                                            rocblaslt_matrix_layout layoutA,
                                            const void* beta, /* host or device pointer */
                                            const void* B,
                                            rocblaslt_matrix_layout layoutB,
                                            void* C,
                                            rocblaslt_matrix_layout layoutC,
                                            hipStream_t stream)
{
    auto key = std::make_tuple(layoutA->type, desc->scaleType, layoutA->order, layoutB->order, layoutC->order);

    if (!transformKernels.count(key)) {
        return rocblaslt_status_internal_error;
    }

    bool transA = desc->opA == HIPBLAS_OP_T;
    bool transB = desc->opB == HIPBLAS_OP_T;

    transformKernels.at(key)(C, A, B, alpha, beta, layoutC->m, layoutC->n, layoutA->ld, layoutB->ld, layoutC->ld, layoutA->batch_count, layoutA->batch_stride, transA, transB, stream);

    return rocblaslt_status_success;
}