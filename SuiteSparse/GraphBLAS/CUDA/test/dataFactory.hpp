// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cmath>
#include <cstdint>
#include <random>
#include <unordered_set>

#include "GB.h"
#include "../type_convert.hpp"
#include "../GB_Matrix_allocate.h"
#include "test_utility.hpp"

static const char *_cudaGetErrorEnum(cudaError_t error) {
  return cudaGetErrorName(error);
}

template <typename T>
void check(T result, char const *const func, const char *const file,
           int const line) {
  if (result) {
    fprintf(stderr, "CUDA error at %s:%d code=%d(%s) \"%s\" \n", file, line,
            static_cast<unsigned int>(result), _cudaGetErrorEnum(result), func);
    exit(EXIT_FAILURE);
  }
}

#define checkCudaErrors(val) check((val), #val, __FILE__, __LINE__)

// This will output the proper error string when calling cudaGetLastError
#define getLastCudaError(msg) __getLastCudaError(msg, __FILE__, __LINE__)

inline void __getLastCudaError(const char *errorMessage, const char *file,
                               const int line) {
  cudaError_t err = cudaGetLastError();

  if (cudaSuccess != err) {
    fprintf(stderr,
            "%s(%i) : getLastCudaError() CUDA error :"
            " %s : (%d) %s.\n",
            file, line, errorMessage, static_cast<int>(err),
            cudaGetErrorString(err));
    exit(EXIT_FAILURE);
  }
}

// This will only print the proper error string when calling cudaGetLastError
// but not exit program incase error detected.
#define printLastCudaError(msg) __printLastCudaError(msg, __FILE__, __LINE__)

inline void __printLastCudaError(const char *errorMessage, const char *file,
                                 const int line) {
  cudaError_t err = cudaGetLastError();

  if (cudaSuccess != err) {
    fprintf(stderr,
            "%s(%i) : getLastCudaError() CUDA error :"
            " %s : (%d) %s.\n",
            file, line, errorMessage, static_cast<int>(err),
            cudaGetErrorString(err));
  }
}
#define CHECK_CUDA(call) checkCudaErrors( call )

// CAUTION: This assumes our indices are small enough to fit into a 32-bit int.
inline std::int64_t gen_key(std::int64_t i, std::int64_t j) {
    return (std::int64_t) i << 32 | (std::int64_t) j;
}

//Vector generators
template<typename T>
void fillvector_linear( int N, T *vec) {
   for (int i = 0; i< N; ++i) vec[i] = T(i);
}
template<typename T>
void fillvector_constant( int N, T *vec, T val) {
   for (int i = 0; i< N; ++i) vec[i] = val;
}

// Mix-in class to enable unified memory
class Managed {
public:
  void *operator new(size_t len) {
    void *ptr = nullptr;
    //std::cout<<"in new operator, alloc for "<<len<<" bytes"<<std::endl;
    CHECK_CUDA( cudaMallocManaged( &ptr, len) );
    cudaDeviceSynchronize();
    //std::cout<<"in new operator, sync "<<len<<" bytes"<<std::endl;
    return ptr;
  }

  void operator delete(void *ptr) {
    cudaDeviceSynchronize();
    //std::cout<<"in delete operator, free "<<std::endl;
    CHECK_CUDA( cudaFree(ptr) );
  }
};

// FIXME: We should just be able to get rid of this now.
//Basic matrix container class
template<typename T>
class matrix : public Managed {
    int64_t nrows_;
    int64_t ncols_;

  public:
    GrB_Matrix mat;

    matrix(int64_t nrows, int64_t ncols): nrows_(nrows), ncols_(ncols) {}

     GrB_Matrix get_grb_matrix() {
         return mat;
     }

     uint64_t get_zombie_count() { return mat->nzombies;}

     void clear() {
        GRB_TRY (GrB_Matrix_clear (mat)) ;
     }

     void alloc() {
         GrB_Type type = cuda::to_grb_type<T>();

         GRB_TRY (GrB_Matrix_new (&mat, type, nrows_, ncols_)) ;
         // GxB_Matrix_Option_set (mat, GxB_SPARSITY_CONTROL,
            // GxB_SPARSE) ;
            // or:
            // GxB_HYPERSPARSE, GxB_BITMAP, GxB_FULL

//         mat = GB_Matrix_allocate(
//            type,   /// <<<<<<<BUG HERE, was NULL, which is broken
//            sizeof(T), nrows, ncols, 2, false, false, Nz, -1);
     }


    void fill_random( int64_t nnz, int gxb_sparsity_control, int gxb_format, std::int64_t seed = 12345ULL, T val_min = 0.0, T val_max = 2.0 , bool debug_print = false) {

        std::cout << "inside fill, using seed "<< seed << std::endl;
        alloc();

        double inv_sparsity ;
        if (nnz < 0)
        {
            // build a matrix with all entries present
            inv_sparsity = 1 ;
        }
        else
        {
            inv_sparsity = ceil(((double)nrows_*ncols_)/nnz);   //= values not taken per value occupied in index space
        }

        std::cout<< "fill_random nrows="<< nrows_<<"ncols=" << ncols_ <<" need "<< nnz<<" values, invsparse = "<<inv_sparsity<<std::endl;
        std::cout<< "fill_random"<<" after alloc values"<<std::endl;
        std::cout<<"vdim ready "<<std::endl;
        std::cout<<"vlen ready "<<std::endl;
        std::cout<<"ready to fill p"<<std::endl;

        bool make_symmetric = false;
        bool no_self_edges = false;

        std::mt19937 r(seed);
        std::uniform_real_distribution<double> dis(0.0, 1.0);

        if (nnz < 0)
        {
            for (int64_t i = 0 ; i < nrows_ ; i++)
            {
                for (int64_t j = 0 ; j < ncols_ ; j++)
                {
                    T x = (T)(dis(r) * (val_max - val_min)) + (T)val_min ;
                    if (make_symmetric)
                    {
                        // A (i,j) = x
                        cuda::set_element<T> (mat, x, i, j) ;
                        // A (j,i) = x
                        cuda::set_element<T> (mat, x, j, i) ;
                    }
                    else
                    {
                        // A (i,j) = x
                        cuda::set_element<T> (mat, x, i, j) ;
                    }
                }
            }
        }
        else
        {
            unordered_set<std::int64_t> key_lookup;

            while(key_lookup.size() < nnz) {
                GrB_Index i = ((GrB_Index) (dis(r) * nrows_)) % ((GrB_Index) nrows_) ;
                GrB_Index j = ((GrB_Index) (dis(r) * ncols_)) % ((GrB_Index) ncols_) ;

                key_lookup.insert(gen_key(i, j));
            }

            for (int64_t k : key_lookup)
            {
                GrB_Index i = k >> 32;
                GrB_Index j = k & 0x0000ffff;

                if (no_self_edges && (i == j)) continue ;
                T x = (T)(dis(r) * (val_max - val_min)) + (T)val_min ;
                // A (i,j) = x
                cuda::set_element<T> (mat, x, i, j) ;
                if (make_symmetric) {
                    // A (j,i) = x
                    cuda::set_element<T>(mat, x, j, i) ;
                }
            }
        }

        GRB_TRY (GrB_Matrix_wait (mat, GrB_MATERIALIZE)) ;
        GB_convert_any_to_non_iso (mat, true, NULL) ;
        // TODO: Need to specify these
        GRB_TRY (GxB_Matrix_Option_set (mat, GxB_SPARSITY_CONTROL, gxb_sparsity_control)) ;
        GRB_TRY (GxB_Matrix_Option_set(mat, GxB_FORMAT, gxb_format));
        GRB_TRY (GrB_Matrix_nvals ((GrB_Index *) &nnz, mat)) ;
        GRB_TRY (GxB_Matrix_fprint (mat, "my mat", GxB_SHORT_VERBOSE, stdout)) ;

        bool iso ;
        GRB_TRY (GxB_Matrix_iso (&iso, mat)) ;
        if (iso)
        {
            printf ("Die! (cannot do iso)\n") ;
            GRB_TRY (GrB_Matrix_free (&mat)) ;
        }

    }

};

template< typename T_C, typename T_M, typename T_A, typename T_B>
class SpGEMM_problem_generator {

    float Anzpercent,Bnzpercent,Cnzpercent;
    int64_t Cnz;
    int64_t *Bucket = nullptr;

    int64_t BucketStart[13];
    unsigned seed = 13372801;
    bool ready = false;

    int64_t nrows_;
    int64_t ncols_;

  public:

    matrix<T_C> *C= nullptr;
    matrix<T_M> *M= nullptr;
    matrix<T_A> *A= nullptr;
    matrix<T_B> *B= nullptr;

    SpGEMM_problem_generator(int64_t nrows, int64_t ncols): nrows_(nrows), ncols_(ncols) {
    
       // Create sparse matrices
       C = new matrix<T_C>(nrows_, ncols_);
       M = new matrix<T_M>(nrows_, ncols_);
       A = new matrix<T_A>(nrows_, ncols_);
       B = new matrix<T_B>(nrows_, ncols_);
    };

    matrix<T_C>* getCptr(){ return C;}
    matrix<T_M>* getMptr(){ return M;}
    matrix<T_A>* getAptr(){ return A;}
    matrix<T_B>* getBptr(){ return B;}

    void init_A(std::int64_t Anz, int gxb_sparsity_control, int gxb_format, std::int64_t seed = 12345ULL, T_A min_val = 0.0, T_A max_val = 2.0) {
        Anzpercent = float(Anz)/float(nrows_*ncols_);
        A->fill_random(Anz, gxb_sparsity_control, gxb_format, seed, min_val, max_val);
    }

    void init_B(std::int64_t Bnz, int gxb_sparsity_control, int gxb_format, std::int64_t seed = 54321ULL, T_B min_val = 0.0, T_B max_val = 2.0) {
        Bnzpercent = float(Bnz)/float(nrows_*ncols_);
        B->fill_random(Bnz, gxb_sparsity_control, gxb_format, seed, min_val, max_val);
    }

    GrB_Matrix getC(){ return C->get_grb_matrix();}
    GrB_Matrix getM(){ return M->get_grb_matrix();}
    GrB_Matrix getA(){ return A->get_grb_matrix();}
    GrB_Matrix getB(){ return B->get_grb_matrix();}

    int64_t* getBucket() { return Bucket;}
    int64_t* getBucketStart(){ return BucketStart;}

    void loadCj() {

       // Load C_i with column j info to avoid another lookup
       for (int c = 0 ; c< M->mat->vdim; ++c) {
           for ( int r = M->mat->p[c]; r< M->mat->p[c+1]; ++r){
               C->mat->i[r] = c << 4 ; //shift to store bucket info
           }
       }
    }

    void init_C(float Cnzp, std::int64_t seed_c = 23456ULL, std::int64_t seed_m = 4567ULL){

       // Get sizes relative to fully dense matrices
       Cnzpercent = Cnzp;
       Cnz = (int64_t)(Cnzp * nrows_ * ncols_);

       //Seed the generator
       std::cout<<"filling matrices"<<std::endl;

       C->fill_random(Cnz, GxB_SPARSE, GxB_BY_ROW, seed_m);
       M->fill_random(Cnz, GxB_SPARSE, GxB_BY_ROW, seed_m);

//       std::cout<<"fill complete"<<std::endl;
//       C->mat->p = M->mat->p; //same column pointers (assuming CSC here)
//       C->mat->p_shallow = true ; // C->mat does not own M->mat->p
    }

    void del(){
       C->clear();
       M->clear();
       A->clear();
       B->clear();
       if (Bucket != nullptr) CHECK_CUDA( cudaFree(Bucket) );
       delete C;
       delete M;
       delete A;
       delete B;
       CHECK_CUDA( cudaDeviceSynchronize() );
    }

    void fill_buckets( int fill_bucket){

       std::cout<<Cnz<<" slots to fill"<<std::endl;

       if (fill_bucket == -1){  

       // Allocate Bucket space
       CHECK_CUDA( cudaMallocManaged((void**)&Bucket, Cnz*sizeof(int64_t)) );

       //Fill buckets with random extents such that they sum to Cnz, set BucketStart
           BucketStart[0] = 0; 
           BucketStart[12] = Cnz;
           for (int b = 1; b < 12; ++b){
              BucketStart[b] = BucketStart[b-1] + (Cnz / 12);
              //std::cout<< "bucket "<< b<<" starts at "<<BucketStart[b]<<std::endl;
              for (int j = BucketStart[b-1]; j < BucketStart[b]; ++j) { 
                Bucket[j] = b ; 
              }
           }
           int b = 11;
           for (int j = BucketStart[11]; j < BucketStart[12]; ++j) { 
                Bucket[j] = b ; 
           }
       }
       else {// all in one test bucket
           Bucket = nullptr;
           BucketStart[0] = 0; 
           BucketStart[12] = Cnz;
           for (int b= 0; b<12; ++b){
              if (b <= fill_bucket) BucketStart[b] = 0;
              if (b  > fill_bucket) BucketStart[b] = Cnz;
              //std::cout<< " one  bucket "<< b<<"starts at "<<BucketStart[b]<<std::endl;
           } 
           std::cout<<"all pairs to bucket "<<fill_bucket<<", no filling"<<std::endl;
           std::cout<<"done assigning buckets"<<std::endl;
       }
    }
};


