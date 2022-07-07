#pragma once
#include <cstdint>
#ifndef CUCKOO_HH_H
#define CUCKOO_HH_H

#include <emmintrin.h>
#include "xxhash.h"
#include <unordered_map>
#include <iostream>
#include <chrono>
#include <algorithm>
#include <queue>
#include "RngFast.hpp"
#include "MurmurHash3.h"

using namespace std;

__m256i m1;                                            // multiplier used in fast division
__m256i d1;
uint32_t shf1;
uint32_t shf2;

__m256i m11;                                            // multiplier used in fast division
__m256i d11;
uint32_t shf11;
uint32_t shf12;



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


constexpr int hh_nr_bins = 1 << 18;

template<class K, class V>
class Cuckoo_waterLevel_HH_no_FP_SIMD_256 {
private:
	K _keys[hh_nr_bins][8] = {};
// 	V _values[hh_nr_bins][8] = {};
	V _water_level_plus_1;
	__m256i _wl_item;
//     __m256i _wl_item_minus;
    int min_int;
	int _insertions_till_maintenance;
	int _insertions_till_maintenance_restart;
	int counters_max_load;
    int counter;
	int _seed1;
	int _seed2;
	int _seed3;
    int q;
	int _mask;
    int _max_possible_maintenances;
    int _Z;
    int _k;
    int32_t countersNumber;
    double _alpha;
    long double _delta;
    float _gamma;
    int _delta_update_interval;
    int _maintenances_Until_delta_update;
    uint64_t _coinFlips;
    int _coinFlips_index;
    U256 eight_samples;
    rng::rng128 gen_arr;
    
    
    void updateZK();
    void get8samples();
//     void get8samples1();
	void move_to_bin(K key, V value, int bin);
	void set_water_level(V wl);
	void maintenance();
    void maintenance1();
    void maintenance2();
    void print();
public:
	V _water_level;
	void test_correctness();
	void test_speed();
	Cuckoo_waterLevel_HH_no_FP_SIMD_256(int seed1, int seed2, int seed3, float max_load,float gamma,int n);
	~Cuckoo_waterLevel_HH_no_FP_SIMD_256();
	void insert(K key, V value);
	V query(K key);
};


template<class K, class V>
Cuckoo_waterLevel_HH_no_FP_SIMD_256<K, V>::Cuckoo_waterLevel_HH_no_FP_SIMD_256(int seed1, int seed2, int seed3, float max_load, float gamma, int n) {

//     max_load = max_load/(1+gamma);
    
    
//     _max_int = _mm256_set1_epi32((int)2147483646);
//     min_int = -2147483647;

//     for(int i=0;i<hh_nr_bins;i++){
//         for(int j=0;j<4;j++){
//             _keys[i][j+4] = min_int;
//         }
//     }
//     
    
    
    _water_level = 0;
	_water_level_plus_1 = _water_level + 1;
	_wl_item = _mm256_set1_epi32((int)_water_level_plus_1);
//     _wl_item_minus = _mm256_sub_epi32(_wl_item,_max_int);
    
    
    
    countersNumber = hh_nr_bins*4;
    q = (countersNumber*max_load)/(1+gamma);
    
    
    _alpha = 0.83;
	_seed1 = seed1;
	_seed2 = seed2;
	_seed3 = seed3;
    counter=0;
	_mask = hh_nr_bins - 1;

    
    
    double eta = 0.25 * (pow(_alpha+1,2) + (_alpha -1)*sqrt(pow(_alpha,2) + 14*_alpha + 1) );
	counters_max_load = countersNumber * max_load;
	_insertions_till_maintenance_restart = q * gamma * (eta);
	_insertions_till_maintenance = counters_max_load;
//     cout << "insertions: "<< _insertions_till_maintenance <<endl;
//     cout << "r: "<< _insertions_till_maintenance_restart <<endl;
	srand(42);
    
    
    
    
    _delta = 0.05;
    _gamma = gamma;
    
    _coinFlips = gen_arr();
    _coinFlips_index =-1;
    
    
    
//     _max_possible_maintenances = (n-_max_load)/ _insertions_till_maintenance_restart;
//     cout<< _max_possible_maintenances << endl;
//     _delta /= _max_possible_maintenances;
    
    _delta_update_interval = 100;
    _maintenances_Until_delta_update = _delta_update_interval;
    _delta /= 2* _delta_update_interval;
    
//     _k = ceil(((_alpha * _gamma * (2 + _gamma - _alpha * _gamma)) / (pow(_gamma - _alpha * _gamma, 2))) * log(1 / _delta));
    _k = (2* _alpha) * log(2/_delta)/pow(1 - _alpha,2);
    _Z = (int)((_k * (1 + _gamma)) / (_alpha * _gamma));
//     _Z = 1024;
    if (_Z & 0b111) // integer multiple of 8
        _Z = _Z - (_Z & 0b111) + 8;
    _k = _Z * (_alpha * _gamma) / (1 + _gamma)+0.5;
    
    initFastMod(hh_nr_bins*4);
}

template<class K, class V>
Cuckoo_waterLevel_HH_no_FP_SIMD_256<K, V>::~Cuckoo_waterLevel_HH_no_FP_SIMD_256() {

}


template<class K, class V>
void Cuckoo_waterLevel_HH_no_FP_SIMD_256<K, V>::print(){
    
//     for(int i=0;i<hh_nr_bins;i++){
//             printf("\n");
//             for(int j=0; j<4 ; j++){
//                 printf("[ %d ] : [ %lu ]\n",(int) (_keys[i][j]),_keys[i][j+4]);
//             }
//         }
//         printf("\n\n");
    
}


template<class K, class V>
void Cuckoo_waterLevel_HH_no_FP_SIMD_256<K, V>::insert(K key, V value)
{
// 	uint64_t hash = XXH64((void *) &key, sizeof(key), _seed1);
    uint64_t hash = fmix64(std::hash<uint64_t>()(key));
	uint64_t i = 1;
	uint64_t bin1 = hash & _mask;
	int empty_spot_in_bin1 = -1;
	int empty_spot_in_bin2 = -1;
    
    
	//const __m256i wl_item = _mm256_set1_epi32(_water_level_plus_1);
    
    
    
	const __m512i item = _mm512_set1_epi64((int)key);

    __m512i bin1Content = *((__m512i*) _keys[bin1]);
    
    
    const __mmask8 match1 = _mm512_cmpeq_epu64_mask(item, bin1Content);
    
    
    
// 	const __m512i match1 = _mm256_cmpeq_epi32(item, bin1Content);
// 	const int     mask1 = _mm256_movemask_epi8(match1) & 0xffff;
    
    
    
    
	if (match1 != 0) {
		int tz1 = _tzcnt_u32(mask1);
// 		_values[bin1][tz1 >> 2] += value;
        _keys[bin1][4+(tz1 >> 2)] += value;
        
        
//         print();
		return;
	}

	uint64_t bin2 = (hash >> 32) & _mask;
   
    
    __m256i bin2Content = *((__m256i*) _keys[bin2]);
    
	const __m256i match2 = _mm256_cmpeq_epi32(item, bin2Content);
	const int     mask2 = _mm256_movemask_epi8(match2) & 0xffff;
	if (mask2 != 0) {
		int tz2 = _tzcnt_u32(mask2);
// 		_values[bin2][tz2 >> 2] += value;
        _keys[bin2][4+(tz2 >> 2)] += value;
        
        
//         print();
		return;
	}

	if (!--_insertions_till_maintenance) {
		maintenance1();
	}
	
	

//     __m256i bin1Content_minus = _mm256_sub_epi32(bin1Content,_max_int);
 
    
//     const __m256i wl_match1 = _mm256_cmpgt_epi32(_wl_item_minus, bin1Content_minus);
    const __m256i wl_match1 = _mm256_cmpgt_epi32(_wl_item, bin1Content);
	const int     wl_mask1 = _mm256_movemask_epi8(wl_match1) >> 16;
    

	// Should we add deltas?
	if (wl_mask1 != 0) {
        
		int tz1 = _tzcnt_u32(wl_mask1);
		empty_spot_in_bin1 = tz1 >> 2;
		_keys[bin1][empty_spot_in_bin1] = key;
// 		_values[bin1][empty_spot_in_bin1] = _water_level+value;
        _keys[bin1][4+empty_spot_in_bin1] = _water_level+value;
    
        
//         print();
		return;
	}
	


	//uint64_t bin2 = (hash >> 32) & _mask;
	
// 	    __m256i bin2Content_minus = _mm256_sub_epi32(bin2Content,_max_int);
        
        



//     const __m256i wl_match2 = _mm256_cmpgt_epi32(_wl_item, bin2Content);
//         const __m256i wl_match2 = _mm256_cmpgt_epi32(_wl_item_minus, bin2Content_minus);
        
        
        
    const __m256i wl_match2 = _mm256_cmpgt_epi32(_wl_item, bin2Content);
	const int     wl_mask2 = _mm256_movemask_epi8(wl_match2) >> 16 ;
    
	if (wl_mask2 != 0) {
		int tz2 = _tzcnt_u32(wl_mask2);
		empty_spot_in_bin2 = tz2 >> 2;
		_keys[bin2][empty_spot_in_bin2] = key;
// 		_values[bin2][empty_spot_in_bin2] = _water_level + value;
        _keys[bin2][empty_spot_in_bin2+4] = _water_level + value;
//         print();
		return;
	}
	
	K kicked_key;
	V kicked_value;
	uint64_t kicked_from_bin;
//  	int coinFlip = rand();
    
    if(_coinFlips_index > 61){
        _coinFlips = gen_arr();
        _coinFlips_index =0;
    }
    uint64_t coinFlip = _coinFlips >> _coinFlips_index;
    _coinFlips_index +=3;
    
	if (coinFlip & 0b1) {
		uint64_t kicked_index = (coinFlip >> 1) & 0b11;
		kicked_key = _keys[bin1][kicked_index];
// 		kicked_value = _values[bin1][kicked_index];
        kicked_value = _keys[bin1][kicked_index+4];

		_keys[bin1][kicked_index] = key;
// 		_values[bin1][kicked_index] = _water_level + value;
        _keys[bin1][kicked_index+4] = _water_level + value;
		kicked_from_bin = bin1;
		//insert(kicked_key, kicked_value);
	}
	else {
		uint64_t kicked_index = (coinFlip >> 1) & 0b11;
		kicked_key = _keys[bin2][kicked_index];
// 		kicked_value = _values[bin2][kicked_index];
        kicked_value = _keys[bin2][4+kicked_index];
		_keys[bin2][kicked_index] = key;
// 		_values[bin2][kicked_index] = _water_level + value;
        _keys[bin2][kicked_index+4] = _water_level + value;
		kicked_from_bin = bin2;
	}
// 	uint64_t kicked_hash = XXH64((void *) &kicked_key, sizeof(kicked_key), _seed1);
    uint64_t kicked_hash = fmix64(std::hash<uint64_t>()(kicked_key));
	uint64_t kicked_bin_1 = kicked_hash & _mask;

    
    
//     printf("in\n");
	
//     print();
    
    if (kicked_bin_1 != kicked_from_bin) {
		move_to_bin(kicked_key, kicked_value, kicked_bin_1);
	}
	else {
		uint64_t kicked_bin_2 = (kicked_hash >> 32) & _mask;
		move_to_bin(kicked_key, kicked_value, kicked_bin_2);
	}
	
// 	print();
	
// 	printf("out\n");

}







template<class K, class V>
void Cuckoo_waterLevel_HH_no_FP_SIMD_256<K, V>::move_to_bin(K key, V value, int bin )
{

    
//     __m256i content_minus = _mm256_sub_epi32(*((__m256i*) _keys[bin]),_max_int);
//     const __m256i wl_match = _mm256_cmpgt_epi32(_wl_item_minus, content_minus);
    
    
// 	const __m256i wl_match = _mm256_cmpgt_epi32(_wl_item, *((__m256i*) _values[bin]));
    const __m256i wl_match = _mm256_cmpgt_epi32(_wl_item, *((__m256i*) _keys[bin]));
    
    
    
    
	const int     wl_mask = _mm256_movemask_epi8(wl_match) >> 16;
    
//     cout << wl_mask << "       waterLVL: " << _water_level  << "        values: " << _keys[bin][4] << "  " << _keys[bin][5] << "  "<< _keys[bin][6] << "  "<< _keys[bin][7] << endl;
    
	if (wl_mask != 0) {
		int tz = _tzcnt_u32(wl_mask);
		int empty_spot_in_bin = tz >> 2;
		_keys[bin][empty_spot_in_bin] = key;
// 		_values[bin][empty_spot_in_bin] = value;
        _keys[bin][empty_spot_in_bin+4] = value;
//         counter = 0;
		return;
	}

// 	int kicked_index = rand() & 0b11;

    if(_coinFlips_index >62){
        _coinFlips = gen_arr();
        _coinFlips_index =0;
    }
    int kicked_index = (_coinFlips >> _coinFlips_index) & 0b11;
    _coinFlips_index+=2;

    
	K kicked_key = _keys[bin][kicked_index];
// 	V kicked_value = _values[bin][kicked_index];
    V kicked_value = _keys[bin][4+kicked_index];
	_keys[bin][kicked_index] = key;
// 	_values[bin][kicked_index] = value;
    _keys[bin][kicked_index+4] = value;

//     uint64_t kicked_hash = XXH64((void *) &kicked_key, sizeof(kicked_key), _seed1);
    uint64_t kicked_hash = fmix64(std::hash<uint64_t>()(kicked_key));
	uint64_t kicked_bin_1 = kicked_hash & _mask;

	uint64_t kicked_from_bin = bin;

	if (kicked_bin_1 != kicked_from_bin) {
		move_to_bin(kicked_key, kicked_value, kicked_bin_1);
	}
	else {
		uint64_t kicked_bin_2 = (kicked_hash >> 32) & _mask;
		move_to_bin(kicked_key, kicked_value, kicked_bin_2);
	}
}

template<class K, class V>
V Cuckoo_waterLevel_HH_no_FP_SIMD_256<K, V>::query(K key)
{
//     cout << "in" << endl;
//     uint64_t hash = XXH64((void *) &key, sizeof(key), _seed1);
    uint64_t hash = fmix64(std::hash<uint64_t>()(key));
	uint64_t bin1 = hash & _mask;
	const __m256i item = _mm256_set1_epi32((int)key);
	const __m256i match1 = _mm256_cmpeq_epi32(item, *((__m256i*) _keys[bin1]));
	const int mask = _mm256_movemask_epi8(match1) & 0xffff;
	if (mask != 0) {
		int tz1 = _tzcnt_u32(mask);
// 		return _values[bin1][tz1 >> 2];
//         cout << "out" << endl;
        
        return _keys[bin1][4+(tz1 >> 2)] - min_int;
	}
	uint64_t bin2 = (hash >> 32) & _mask;
	const __m256i match2 = _mm256_cmpeq_epi32(item, *((__m256i*) _keys[bin2]));
	const int mask2 = _mm256_movemask_epi8(match2) & 0xffff;
	if (mask2 != 0) {
		//cout << tz1 << endl;
		int tz2 = _tzcnt_u32(mask2);
// 		return _values[bin2][tz2 >> 2];
//         cout << "out" << endl;
        return _keys[bin2][4+(tz2 >> 2)] - min_int;
	}
// 	cout << "out" << endl;
	return _water_level - min_int;
// return 0;
}


template<class K, class V>
inline void Cuckoo_waterLevel_HH_no_FP_SIMD_256<K, V>::set_water_level(V wl)
{
	_water_level = wl;
	_water_level_plus_1 = _water_level + 1;
	_wl_item = _mm256_set1_epi32(_water_level_plus_1);
//     _wl_item_minus = _mm256_sub_epi32(_wl_item,_max_int);
}





template<class K, class V>
void Cuckoo_waterLevel_HH_no_FP_SIMD_256<K, V>::get8samples() {
    int indx = 0;
    eight_samples.ymm = _mm256_set_epi64x(gen_arr(), gen_arr(), gen_arr(), gen_arr());
    __m256i t1 = _mm256_mul_epu32(eight_samples.ymm, m1);                           // 32x32->64 bit unsigned multiplication of even ele/*ment*/s of a
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




template<class K, class V>
inline void Cuckoo_waterLevel_HH_no_FP_SIMD_256<K, V>::maintenance2()
{

    int Z = _Z; 
    int k = _k;
    if(q > _k){
        Z = countersNumber;
        k = q;
    }
    
    V p[Z];

    int sum=0;
    V top = 0;
    int flag =0;
    for(int i=0;i<hh_nr_bins & sum < Z;i++){
            for(int j=0;j<4 & sum < Z;j++){
                p[sum] = (_keys[i][4+j]);
                sum++;
            }
    }
    
    
     std::nth_element(p, p + k, p+(Z-1) );
    
    top = p[k];
    if(top > _water_level){
        set_water_level(top);
    }
    _insertions_till_maintenance = _insertions_till_maintenance_restart;
    
    
        if (--_maintenances_Until_delta_update==0){
            _delta_update_interval *= 2;
            _maintenances_Until_delta_update = _delta_update_interval;
            _delta/=4;
//             updateZK();
    }
    
    counter+=1;
    
}




template<class K, class V>
inline void Cuckoo_waterLevel_HH_no_FP_SIMD_256<K, V>::maintenance1()
{
    
    int Z = _Z; 
    int k = _k;
    if(q > _k){
        Z = countersNumber;
        k = q;
    }
    
    V p[Z];
    int c=0;
    int left_to_fill = Z;
    while (left_to_fill > 0) {
        get8samples();
        for (int i = 0; i < 8; ++i) {
            
            int a  = (int) (eight_samples.i[i] & 3);
            int b  = (int) (eight_samples.i[i] >>2);
            V smpl = _keys[b][4+a];
            p[c] = (smpl);
            c++;
        }
        left_to_fill -= 8;
    }
    
    std::nth_element(p, p + k, p+(Z-1) );
    V top = p[k];
    
    if(top > _water_level){
        set_water_level(top);
    }
	_insertions_till_maintenance = _insertions_till_maintenance_restart;
    
    
    if (--_maintenances_Until_delta_update==0){
            _delta_update_interval *= 2;
            _maintenances_Until_delta_update = _delta_update_interval;
            _delta/=4;
            updateZK();
    }
    
    counter+=1;
    
    
//     int SUM=0;
//     for(int i=0;i<hh_nr_bins;i++){
//             for(int j=0;j<4;j++){
//                 if(_keys[i][j+4] < _water_level){
//                     _keys[i][j+4] = min_int;  
//                     _keys[i][j] = 0;
//                 }
//             }
//     }
    
//     printf("############################# %lu ############################# higher query(2) =%lu \n",_water_level, query(2));
    
}






template<class K, class V>
inline void Cuckoo_waterLevel_HH_no_FP_SIMD_256<K, V>::maintenance()
{
//     counter++;
//     cout<< counter <<endl;
/*    
    cout<< "@@@@@@@@@@@@@@@"<<endl;
    cout<< "delta "<< _delta <<endl;
    cout<< "Z " <<_Z <<endl;
    cout<< "k " <<_k <<endl;
    */
    
//     cout<< "maintenance" << endl;
// 	int size = hh_nr_bins * 8;
// 	V copy_of_values[hh_nr_bins][8];
// 	memcpy(copy_of_values, _values, size * sizeof(V));
// 	V* one_array = (V*)copy_of_values;
// 	sort(one_array, one_array + size);
// 	set_water_level(one_array[size >> 1]);
// // 	cout << "water_level = " << _water_level << endl;
// 	_insertions_till_maintenance = _insertions_till_maintenance_restart;
    
    
//     printf("\n\n############################# maintenance #############################\n");
    
    std::priority_queue <V> p;
    int left_to_fill = _k;
    while (left_to_fill > 0) {
        get8samples();
        for (int i = 0; i < 8; ++i) {
            
            int a  = (int) (eight_samples.i[i] & 3);
            int b  = (int) (eight_samples.i[i] >>2);
            
//             printf("\n\nsample=%d   a=%d  b=%d \n\n\n",eight_samples.i[i],a,b);
            V smpl = _keys[b][4+a];
//             cout << smpl << endl;
            p.push(smpl);
            
        }
        left_to_fill -= 8;
    }
    
    int left_to_sample = _Z-_k+ left_to_fill;
    while (left_to_fill++ < 0) {
        p.pop();
    }
    V top = p.top();

    while (left_to_sample > 0) {
        get8samples();
        for (int i = 0; i < 8; ++i) {
            int a  = eight_samples.i[i] & 3;
            int b  = eight_samples.i[i] >> 2;
//             printf("\n\nsample=%d   a=%d  b=%d \n\n\n",eight_samples.i[i],a,b);
            V smpl = _keys[b][4+a];
//             cout << smpl << endl;
            if ( top > smpl) {
                p.pop();
                p.push(smpl);
                
                top = p.top();
            }
        }
        left_to_sample -= 8;
    }
    
    set_water_level(top);
	_insertions_till_maintenance = _insertions_till_maintenance_restart;
    
    
    if (--_maintenances_Until_delta_update==0){
            _delta_update_interval *= 2;
            _maintenances_Until_delta_update = _delta_update_interval;
            _delta/=4;
            updateZK();
    }
    
    counter+=1;
    
    
//     int SUM=0;
    for(int i=0;i<hh_nr_bins;i++){
            for(int j=0;j<4;j++){
                if(_keys[i][j+4] < _water_level){
                    _keys[i][j+4] = min_int;  
                    _keys[i][j] = 0;
                }
            }
    }
    
//     printf("############################# %lu ############################# higher query(2) =%lu \n",_water_level, query(2));
    
}





template<class K, class V>
void Cuckoo_waterLevel_HH_no_FP_SIMD_256<K, V>::updateZK()
{
//     _k=ceil(((_alpha*_gamma*(2+_gamma - _alpha*_gamma)) / (pow(_gamma -_alpha*_gamma,2))) * log(1/_delta));
    _k = (2* _alpha) * log(2/_delta)/pow(1 - _alpha,2);
    _Z = (int)( (_k*(1+_gamma)) / (_alpha*_gamma) );
    if (_Z & 0b111) // integer multiple of 8
        _Z = _Z - (_Z & 0b111) + 8;
    _k = _Z * (_alpha * _gamma) / (1 + _gamma)+0.5;
}



template<class K, class V>
void Cuckoo_waterLevel_HH_no_FP_SIMD_256<K, V>::test_correctness()
{
	unordered_map<K, V> true_map;
	uint64_t S = 0;
	//float eps = 0.01;

	for (int i = 0; i < (1 << 21); ++i) {
		uint32_t id = rand();
		//uint32_t val = (rand() << 15) | rand();
		uint32_t val =  rand();
		true_map[id] += val;
		S += val;
		insert(id, val);
		//cout << "id = " << id << " val = " << val << " query(id) = " << query(id) << endl;
	}
// 	int nr_counters = hh_nr_bins * 8;
// 	float eps = 2.0 / nr_counters;
// 	for (auto it = true_map.begin(); it != true_map.end(); ++it) {
// 		if ((it->second > query(it->first)) || (it->second < query(it->first) - S*eps)) {
// 			cout << it->first << " " << it->second << " " << query(it->first) << endl;
// 			exit(1);
// 		}
// 	}
}

template<class K, class V>
void Cuckoo_waterLevel_HH_no_FP_SIMD_256<K, V>::test_speed()
{
	auto start = chrono::steady_clock::now();
	for (int64_t i = 0; i < 10; ++i)
	{
		for (int64_t j = 0; j < 3.6 * hh_nr_bins; ++j) {
			insert(j, i + j);
		}
		//for (int64_t j = 0; j < 3.6 * hh_nr_bins; ++j) {
		//	try_remove(j);
		//}
	}
	auto end = chrono::steady_clock::now();

	auto time = chrono::duration_cast<chrono::microseconds>(end - start).count();
	cout << "test_speed: Elapsed time in milliseconds : "
		<< time / 1000
		<< " ms" << endl;
}
#endif //CUCKOO_HH_H
