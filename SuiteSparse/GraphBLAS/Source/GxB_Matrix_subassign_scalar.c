//------------------------------------------------------------------------------
// GxB_Matrix_subassign_[SCALAR]: assign to submatrix, via scalar expansion
//------------------------------------------------------------------------------

// SuiteSparse:GraphBLAS, Timothy A. Davis, (c) 2017-2022, All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

//------------------------------------------------------------------------------

// Assigns a single scalar to a submatrix:

// C(Rows,Cols)<M> = accum(C(Rows,Cols),x)

// The scalar x is implicitly expanded into a matrix A of size nRows-by-nCols,
// with each entry in A equal to x.

// Compare with GrB_Matrix_assign_scalar,
// which uses M and C_Replace differently.

#define GB_FREE_ALL ;
#include "GB_subassign.h"
#include "GB_ij.h"
#include "GB_get_mask.h"

#define GB_ASSIGN_SCALAR(type,T,ampersand)                                     \
GrB_Info GB_EVAL2 (GXB (Matrix_subassign_), T) /* C(Rows,Cols)<M> += x      */ \
(                                                                              \
    GrB_Matrix C,                   /* input/output matrix for results      */ \
    const GrB_Matrix M,             /* optional mask for C(Rows,Cols)       */ \
    const GrB_BinaryOp accum,       /* accum for Z=accum(C(Rows,Cols),x)    */ \
    type x,                         /* scalar to assign to C(Rows,Cols)     */ \
    const GrB_Index *Rows,          /* row indices                          */ \
    GrB_Index nRows,                /* number of row indices                */ \
    const GrB_Index *Cols,          /* column indices                       */ \
    GrB_Index nCols,                /* number of column indices             */ \
    const GrB_Descriptor desc       /* descriptor for C(Rows,Cols) and M */    \
)                                                                              \
{                                                                              \
    GB_WHERE (C, "GxB_Matrix_subassign_" GB_STR(T)                             \
        " (C, M, accum, x, Rows, nRows, Cols, nCols, desc)") ;                 \
    GB_BURBLE_START ("GxB_Matrix_subassign " GB_STR(T)) ;                      \
    GB_RETURN_IF_NULL_OR_FAULTY (C) ;                                          \
    GB_RETURN_IF_FAULTY (M) ;                                                  \
    GrB_Info info = GB_subassign_scalar (C, M, accum, ampersand x,             \
        GB_## T ## _code, Rows, nRows, Cols, nCols, desc, Context) ;           \
    GB_BURBLE_END ;                                                            \
    return (info) ;                                                            \
}

GB_ASSIGN_SCALAR (bool      , BOOL   , &)
GB_ASSIGN_SCALAR (int8_t    , INT8   , &)
GB_ASSIGN_SCALAR (uint8_t   , UINT8  , &)
GB_ASSIGN_SCALAR (int16_t   , INT16  , &)
GB_ASSIGN_SCALAR (uint16_t  , UINT16 , &)
GB_ASSIGN_SCALAR (int32_t   , INT32  , &)
GB_ASSIGN_SCALAR (uint32_t  , UINT32 , &)
GB_ASSIGN_SCALAR (int64_t   , INT64  , &)
GB_ASSIGN_SCALAR (uint64_t  , UINT64 , &)
GB_ASSIGN_SCALAR (float     , FP32   , &)
GB_ASSIGN_SCALAR (double    , FP64   , &)
GB_ASSIGN_SCALAR (GxB_FC32_t, FC32   , &)
GB_ASSIGN_SCALAR (GxB_FC64_t, FC64   , &)
GB_ASSIGN_SCALAR (void *    , UDT    ,  )

//------------------------------------------------------------------------------
// GxB_Matrix_subassign_Scalar: subassign a GrB_Scalar to a matrix
//------------------------------------------------------------------------------

// If the GrB_Scalar s is non-empty, then this is the same as the non-opapue
// scalar subassignment above.

// If the GrB_Scalar s is empty of type stype, then this is identical to:
//  GrB_Matrix_new (&S, stype, nRows, nCols) ;
//  GxB_Matrix_subassign (C, M, accum, S, Rows, nRows, Cols, nCols, desc) ;
//  GrB_Matrix_free (&S) ;

#undef  GB_FREE_ALL
#define GB_FREE_ALL GB_Matrix_free (&S) ;
#include "GB_static_header.h"

GB_PUBLIC
GrB_Info GxB_Matrix_subassign_Scalar   // C(I,J)<M> = accum (C(I,J),s)
(
    GrB_Matrix C,                   // input/output matrix for results
    const GrB_Matrix M_in,          // optional mask for C, unused if NULL
    const GrB_BinaryOp accum,       // optional accum for Z=accum(C(I,J),x)
    GrB_Scalar scalar,              // scalar to assign to C(I,J)
    const GrB_Index *I,             // row indices
    GrB_Index ni,                   // number of row indices
    const GrB_Index *J,             // column indices
    GrB_Index nj,                   // number of column indices
    const GrB_Descriptor desc       // descriptor for C and Mask
)
{

    //--------------------------------------------------------------------------
    // check inputs
    //--------------------------------------------------------------------------

    GrB_Matrix S = NULL ;
    GB_WHERE (C, "GxB_Matrix_subassign_Scalar"
        " (C, M, accum, s, Rows, nRows, Cols, nCols, desc)") ;
    GB_BURBLE_START ("GxB_subassign") ;
    GB_RETURN_IF_NULL_OR_FAULTY (C) ;
    GB_RETURN_IF_NULL_OR_FAULTY (scalar) ;
    GB_RETURN_IF_FAULTY (M_in) ;

    // get the descriptor
    GB_GET_DESCRIPTOR (info, desc, C_replace, Mask_comp, Mask_struct,
        xx1, xx2, xx3, xx7) ;

    // get the mask
    GrB_Matrix M = GB_get_mask (M_in, &Mask_comp, &Mask_struct) ;

    //--------------------------------------------------------------------------
    // C(Rows,Cols)<M> = accum (C(Rows,Cols), scalar)
    //--------------------------------------------------------------------------

    GrB_Index nvals ;
    GB_OK (GB_nvals (&nvals, (GrB_Matrix) scalar, Context)) ;
    if (nvals == 1)
    { 

        //----------------------------------------------------------------------
        // the opaque GrB_Scalar has a single entry
        //----------------------------------------------------------------------

        // This is identical to non-opaque scalar subassignment

        info = (GB_subassign (
            C, C_replace,               // C matrix and its descriptor
            M, Mask_comp, Mask_struct,  // mask matrix and its descriptor
            false,                      // do not transpose the mask
            accum,                      // for accum (C(Rows,Cols),scalar)
            NULL, false,                // no explicit matrix A
            I, ni,                      // row indices
            J, nj,                      // column indices
            true,                       // do scalar expansion
            scalar->x,                  // scalar to assign, expands to become A
            scalar->type->code,         // type code of scalar to expand
            Context)) ;

    }
    else
    { 

        //----------------------------------------------------------------------
        // the opaque GrB_Scalar has no entry
        //----------------------------------------------------------------------

        // determine the properites of the I and J index lists
        int64_t nRows, nCols, RowColon [3], ColColon [3] ;
        int RowsKind, ColsKind ;
        GB_ijlength (I, ni, GB_NROWS (C), &nRows, &RowsKind, RowColon);
        GB_ijlength (J, nj, GB_NCOLS (C), &nCols, &ColsKind, ColColon);

        // create an empty matrix S of the right size, and use matrix assign
        struct GB_Matrix_opaque S_header ;
        GB_CLEAR_STATIC_HEADER (S, &S_header) ;
        bool is_csc = C->is_csc ;
        int64_t vlen = is_csc ? nRows : nCols ;
        int64_t vdim = is_csc ? nCols : nRows ;
        GB_OK (GB_new (&S, // existing header
            scalar->type, vlen, vdim, GB_Ap_calloc, is_csc, GxB_AUTO_SPARSITY,
            GB_HYPER_SWITCH_DEFAULT, 1, Context)) ;
        info = GB_subassign (
            C, C_replace,                   // C matrix and its descriptor
            M, Mask_comp, Mask_struct,      // mask matrix and its descriptor
            false,                          // do not transpose the mask
            accum,                          // for accum (C(Rows,Cols),A)
            S, false,                       // S matrix and its descriptor
            I, ni,                          // row indices
            J, nj,                          // column indices
            false, NULL, GB_ignore_code,    // no scalar expansion
            Context) ;
        GB_FREE_ALL ;
    }

    GB_BURBLE_END ;
    return (info) ;
}

