#pragma once
#ifndef S_SQUIDIN
#define S_S_SQUIDIN
#include <fstream>
#include <cstdio>
#include <stdio.h>
#include <assert.h>
#include <iostream>
#include <time.h>
#include <immintrin.h>
#include <iomanip> 
#include "RngFast.hpp"
#include <queue>
#include <bits/stdc++.h>
#include <stdlib.h>
#include <string.h>
#include <unordered_map>
#include <functional>
#include <random>
#include <math.h> 
#include <vector>
#include <bitset>
#include <cstring>
#include <algorithm>
#include <memory.h>
#include <chrono>

using namespace std;


__m256i m1;                                            // multiplier used in fast division
__m256i d1;
uint32_t shf1;
uint32_t shf2;

union U256 {
    int32_t i[8];
    __m256i ymm;
};


void initFastMod(uint32_t d) {                                 // Set or change divisor, calculate parameters
    uint32_t L, L2, sh1, sh2, m;
    switch (d) {
    case 0:
        m = sh1 = sh2 = 1 / d;                         // provoke error for d = 0
        break;
    case 1:
        m = 1; sh1 = sh2 = 0;                          // parameters for d = 1
        break;
    case 2:
        m = 1; sh1 = 1; sh2 = 0;                       // parameters for d = 2
        break;
    default:                                           // general case for d > 2
        L = ceil(log2(d));//bit_scan_reverse(d - 1) + 1;                  // ceil(log2(d))
        L2 = L < 32 ? 1 << L : 0;                      // 2^L, overflow to 0 if L = 32
        m = 1 + uint32_t((uint64_t(L2 - d) << 32) / d); // multiplier
        sh1 = 1;  sh2 = L - 1;                         // shift counts
    }
    m1 = _mm256_set1_epi32(m);
    d1 = _mm256_set1_epi32(d);
    shf1 = sh1;
    shf2 = sh2;
}




template<typename key, typename val>
class SquidIN {
  key *_K;
  val *_V;
  val _phi;
  key _k_phi;
  std::default_random_engine _generator;
  unordered_map<key, int> hash_table;

  int _curIdx;
  int _q;
  int _actualsize;
  int _actualsizeMinusOne;
  int _qMinusOne;
  float _gamma;
  int _nminusq;
  double _delta;
  double _alpha;
  double _psi;
  int _k;
  int _Z;
  rng::rng128 gen_arr;
  int counter;
  U256 eight_samples;
  // 	std::priority_queue <int, std::vector<int>, std::greater<int>> testp;
  int testSize;
  int _correctness_threshold;
  int _min_maintenance_free;
  

  void maintenance(){
    findKthLargestAndPivot();
    hash_table.clear();
    for(int i = _nminusq; i < _actualsize; i++) {
      hash_table[_K[i]] = i;
    }
  }
  
  
  inline void swap(int a, int b){
    if(a==b) return;
    key k = _K[a];
    _K[a] = _K[b];
    _K[b] = k;
    val v = _V[a];
    _V[a] = _V[b];
    _V[b] = v;
  }
  
  
  
  int PartitionAroundPivot(int left, int right, int pivot_idx, val* nums){
    val pivot_value = nums[pivot_idx];
           int i=left, j=right;
        while (i<=j) {
            while (nums[i]<pivot_value) i++;
            while (nums[j]>pivot_value) j--;
            if (i<=j) swap(i++, j--);
        }
        // return new position of pivot
        return i-1;
  }
  

  
  

void get8samples() {
    int indx = 0;
    eight_samples.ymm = _mm256_set_epi64x(gen_arr(), gen_arr(), gen_arr(), gen_arr());
    __m256i t1 = _mm256_mul_epu32(eight_samples.ymm, m1);                           // 32x32->64 bit unsigned multiplication of even elements of a
    __m256i t2 = _mm256_srli_epi64(t1, 32);                         // high dword of even numbered results
    __m256i t3 = _mm256_srli_epi64(eight_samples.ymm, 32);                          // get odd elements of a into position for multiplication
    __m256i t4 = _mm256_mul_epu32(t3, m1);                          // 32x32->64 bit unsigned multiplication of odd elements
    __m256i t5 = _mm256_set_epi32(-1, 0, -1, 0, -1, 0, -1, 0);      // mask for odd elements
    __m256i t7 = _mm256_blendv_epi8(t2, t4, t5);                    // blend two results
    __m256i t8 = _mm256_sub_epi32(eight_samples.ymm, t7);                           // subtract
    __m256i t9 = _mm256_srli_epi32(t8, shf1);                       // shift right logical
    __m256i t10 = _mm256_add_epi32(t7, t9);                         // add
    __m256i t11 = _mm256_srli_epi32(t10, shf2);                     // shift right logical 
    __m256i t12 = _mm256_mullo_epi32(t11, d1);                      // multiply quotient with divisor
    eight_samples.ymm = _mm256_sub_epi32(eight_samples.ymm, t12);                   // subtract
}
  
  
  

// if value dont exist in _K return -1, otherwise return the index of the value in _K
int findValueIndex(val value) {
    for (int i = 0; i < _actualsize; i++) {
        if (_V[i] == value) {
            return i;
        }
    }
    return -1;
}




// if value dont exist in _K return -1, otherwise return the index of the value in _K
int findValueDup(val value,int idx, int check) {
    int sum=check;
    for (int i = idx; i < _actualsize; i++) {
        if (_V[i] == value) {
            sum--;
            if(sum==0){
                return 1;
            }
        }
    }
    return 0;
}


// check if the conditions holds for the possible pivot "value"
// return the pivot index in _A if it hold otherwise return -1
int checkPivot(val value) {
    int left = 0, right = _actualsizeMinusOne;
    int new_pivot_idx;


    int pivot_idx = findValueIndex(value);
//     int dup = findValueDup(value ,pivot_idx);
    if (pivot_idx == -1) {
        return -1;
    }
    
    new_pivot_idx = PartitionAroundPivot(left, right, pivot_idx, _V);

    
    
    

    if (new_pivot_idx <= _correctness_threshold) {
//         if (new_pivot_idx >= _min_maintenance_free) {
            return new_pivot_idx;
//         }
//         else{
// //                 printf("2:  %d\n",new_pivot_idx);
//                 return -1;
//         }
    }
//     else if ( findValueDup(value ,0, new_pivot_idx - _correctness_threshold)==1  ) {
// //         printf("dup %d\n", dup);
//         return _correctness_threshold;
//     }
    
    
//     printf("1:  %d\n",new_pivot_idx);
    return -1*new_pivot_idx;
}

  

  
  
  

void findKthLargestAndPivot() {
    int tries = 2;
    while (tries != 0) {
        std::priority_queue <val> p;
        int left_to_fill = _k;
        while (left_to_fill > 0) {
            get8samples();
            for (int i = 0; i < 8; ++i) {
                p.push(_V[eight_samples.i[i]]);
            }
            left_to_fill -= 8;
        }

        int left_to_sample = _Z-_k+ left_to_fill;
        while (left_to_fill++ < 0) {
            p.pop();
        }
        
        val top = p.top();

        while (left_to_sample > 0) {
            get8samples();
            for (int i = 0; i < 8; ++i) {
                if (top > _V[eight_samples.i[i]]) {
                    p.pop();
                    p.push(_V[eight_samples.i[i]]);
                    top = p.top();
                }
            }
            left_to_sample -= 8;
        }
        int idx = checkPivot(top);
        if (idx > 0) {
            _curIdx = idx;
            _k_phi = _K[idx];
            _phi = _V[idx];
            return;
        }
        else if(_q>100000 and idx < -1){
            int l = -idx - _correctness_threshold;
            int pop = ( (_k-1) - _k * ( (_q*_gamma*_alpha)/(l+(_q*_gamma)) ) );
            for(int i=0;i<pop;i++){
                p.pop();
            }
            top = p.top();
            idx = checkPivot(top);
            if(idx >= 0){
                _curIdx = idx;
                _phi = _V[idx];
                _k_phi = _K[idx];
                return;
            }
        }
        tries--;
    }
    int left = 0, right = _actualsizeMinusOne;
	while (left <= right) {
		int pivot_idx = (left+right)/2;
		int new_pivot_idx = PartitionAroundPivot(left, right, pivot_idx, _V);
		if (new_pivot_idx == _nminusq) {
            _phi = _V[new_pivot_idx];
			_k_phi = _K[new_pivot_idx];
            _curIdx = new_pivot_idx;
            return;
		} else if (new_pivot_idx > _nminusq) {
			right = new_pivot_idx - 1;
		} else {  // new_pivot_idx < _q - 1.
			left = new_pivot_idx + 1;
		}
	}
}
  
  
  
  
  
 public:
     
  template<typename _key, typename _val>
  class outputkv {
  public:
    _key *keyArr;
    _val *valArr;
  };
  
  
  SquidIN(int q, float gamma, key default_key=0, val default_val=0){
    _actualsize = q * (1+gamma);
    _actualsizeMinusOne = _actualsize - 1;
    _curIdx = _actualsize;
    _K = (key*) malloc(sizeof(key) * _actualsize);
    _V = (val*) malloc(sizeof(val) * _actualsize);
    if (!_V || !_K) {
      exit(1);
    }
    _gamma = gamma;
    _q = q;
    _qMinusOne = q - 1;
    _nminusq = _actualsize - q;
    _phi = default_val;
    _k_phi = default_key;
    _delta =0.6;
    _alpha = 0.83;
    _psi = 1.0 / 5.0;
    _k = ceil(((_alpha * _gamma * (2 + _gamma - _alpha * _gamma)) / (pow(_gamma - _alpha * _gamma, 2))) * log(1 / _delta));
    _Z = (int)((_k * (1 + _gamma)) / (_alpha * _gamma));
    if (_Z & 0b111) // integer multiple of 8
        _Z = _Z - (_Z & 0b111) + 8;
    _k = _Z * (_alpha * _gamma) / (1 + _gamma)+0.5;
    
    
    if(q <= 100000){
        _delta =0.99;
        _k = ceil(((_alpha * _gamma * (2 + _gamma - _alpha * _gamma)) / (pow(_gamma - _alpha * _gamma, 2))) * log(1 / _delta));
        _Z = (int)((_k * (1 + _gamma)) / (_alpha * _gamma));
        if (_Z & 0b111) // integer multiple of 8
            _Z = _Z - (_Z & 0b111) + 8;
        _k = _Z * (_alpha * _gamma) / (1 + _gamma)+0.5;
    }
    gen_arr();
    counter = 0;
    initFastMod(_actualsize);
    _correctness_threshold = int(_gamma * q);
    _min_maintenance_free = int(_gamma * q * _psi);
  }
  
  
  
  
  ~SquidIN(){
    free(_K);
    free(_V);
  }
  bool isIn(key k) {
    return hash_table.count(k);
  }
  val getMinimalVal() {
    return _phi;
  }
  key getMinimalKey() {
    return _k_phi;
  }
  void insert(key k, val v){
    if (v < _phi) { // DIRECT COMPARSION: compare if new val is smaller than smallest val
      return;
    } else {
      _K[--_curIdx] = k;
      _V[_curIdx] = v;   
      hash_table[k] = _curIdx;
      if (_curIdx){
        return;
      } else {
        maintenance();
      }
    }

  }
  void update(key k, val v, std::function<void(val&,val&)> f = [](val& v1, val& v2) {v1+=v2;}) {
    int k_idx = hash_table[k];
    f(_V[k_idx], v);
  }
  outputkv<key, val> largestQ(){
    outputkv<key,val> out;
    maintenance();
    out.keyArr = _K + _nminusq;
    out.valArr = _V + _nminusq;
    return out;
  }
};

#endif

