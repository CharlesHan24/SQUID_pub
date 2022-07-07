#pragma once
#pragma once
#ifndef CUCKOO_WL_H
#define CUCKOO_WL_H

#include <cstdint>
#include <emmintrin.h>
#include "xxhash.h"

using namespace std;

//constexpr int bin_width = 4;
//constexpr int nr_bins = 1 << 10;



template<class K, class V>
class HH_Cuckoo_no_FP_SIMD {
private:
	K _keys[nr_bins][bin_width] = {};
	V _values[nr_bins][bin_width] = {};
	V _water_level;
	__m128i water_level_item;

	int _seed1;
	int _seed2;
	int _seed3;

	int _mask;

	void move_to_bin(K key, V value, int bin);

	//xxh::xxhash3<64>(str, FT_SIZE, seeds[0]);
public:
	void test_correctness();
	void test_speed();
	HH_Cuckoo_no_FP_SIMD(int seed1, int seed2, int seed3);
	~HH_Cuckoo_no_FP_SIMD();
	//void insert(K key, V value);
	void add(K key, V value);
	V query(K key);
	void try_remove(K key);
	V get_water_level() {
		return _water_level;
	};
	void set_water_level(V water_level) {
		_water_level = water_level;
		water_level_item = _mm_set1_epi32(water_level);
	}
};


template<class K, class V>
HH_Cuckoo_no_FP_SIMD<K, V>::HH_Cuckoo_no_FP_SIMD(int seed1, int seed2, int seed3) {
	_seed1 = seed1;
	_seed2 = seed2;
	_seed3 = seed3;
	_mask = 0x3FF;
	_water_level = 0;
	srand(42);
}

template<class K, class V>
HH_Cuckoo_no_FP_SIMD<K, V>::~HH_Cuckoo_no_FP_SIMD() {

}

template<class K, class V>
void HH_Cuckoo_no_FP_SIMD<K, V>::add(K key, V value)
{
	uint64_t hash = xxh::xxhash3<64>(&key, sizeof(key), _seed1);
	uint64_t i = 1;
	uint64_t bin1 = hash & _mask;
	int empty_spot_in_bin1 = -1;
	int empty_spot_in_bin2 = -1;

	const __m128i zero_item = _mm_set1_epi32(0);
	const __m128i item = _mm_set1_epi32((int)key);

	const __m128i match1 = _mm_cmpeq_epi32(item, *((__m128i*) _keys[bin1]));
	const int     mask1 = _mm_movemask_epi8(match1);
	if (mask1 != 0) {
		int tz1 = _tzcnt_u32(mask1);
		_values[bin1][tz1 >> 2] += value;
		return;
	}
	uint64_t bin2 = (hash >> 32) & _mask;

	const __m128i match2 = _mm_cmpeq_epi32(item, *((__m128i*) _keys[bin2]));
	const int     mask2 = _mm_movemask_epi8(match2);
	if (mask2 != 0) {
		int tz2 = _tzcnt_u32(mask2);
		_values[bin2][tz2 >> 2] += value;
		return;
	}
	const __m128i WL_match2 = _mm_cmple_epi32(water_level_item, *((__m128i*) _keys[bin2]));
	const int     WL_mask2 = _mm_movemask_epi8(water_level_item);
	if (WL_mask2 != 0) {
		int tz2 = _tzcnt_u32(WL_mask2);
		empty_spot_in_bin2 = tz2 >> 2;
	}

	const __m128i WL_match1 = _mm_cmple_epi32(water_level_item, *((__m128i*) _keys[bin1]));
	const int     WL_mask1 = _mm_movemask_epi8(water_level_item);
	if (WL_mask1 != 0) {
		int tz1 = _tzcnt_u32(WL_mask1);
		empty_spot_in_bin1 = tz1 >> 2;
	}



	for (int i = bin_width - 1; i >= 0; --i) {
		if (_keys[bin2][i] == key) {
			_values[bin2][i] = value;
			return;
		}
		if (_keys[bin2][i] == 0) {
			empty_spot_in_bin2 = i;
		}
	}
	if (empty_spot_in_bin1 != -1) {
		_keys[bin1][empty_spot_in_bin1] = key;
		_values[bin1][empty_spot_in_bin1] = value;
		return;
	}
	if (empty_spot_in_bin2 != -1) {
		_keys[bin2][empty_spot_in_bin2] = key;
		_values[bin2][empty_spot_in_bin2] = value;
		return;
	}
	K kicked_key;
	V kicked_value;
	uint64_t kicked_from_bin;
	int coinFlip = rand();
	if (coinFlip & 0b1) {
		int kicked_index = (coinFlip >> 1) & 0b11;
		kicked_key = _keys[bin1][kicked_index];
		kicked_value = _values[bin1][kicked_index];

		_keys[bin1][kicked_index] = key;
		_values[bin1][kicked_index] = value;
		kicked_from_bin = bin1;
		//insert(kicked_key, kicked_value);
	}
	else {
		int kicked_index = (coinFlip >> 1) & 0b11;
		kicked_key = _keys[bin2][kicked_index];
		kicked_value = _values[bin2][kicked_index];
		_keys[bin2][kicked_index] = key;
		_values[bin2][kicked_index] = value;
		kicked_from_bin = bin2;
	}
	uint64_t kicked_hash = xxh::xxhash3<64>(&kicked_key, sizeof(kicked_key), _seed1);
	uint64_t kicked_bin_1 = kicked_hash & _mask;

	if (kicked_bin_1 != kicked_from_bin) {
		move_to_bin(kicked_key, kicked_value, kicked_bin_1);
	}
	else {
		uint64_t kicked_bin_2 = (kicked_hash >> 32) & _mask;
		move_to_bin(kicked_key, kicked_value, kicked_bin_2);
	}
}

template<class K, class V>
void HH_Cuckoo_no_FP_SIMD<K, V>::move_to_bin(K key, V value, int bin)
{
	//cout << "move_to_bin" << endl;
	/*for (int i = bin_width - 1; i >= 0; --i) {
		if (_keys[bin][i] == 0) {
			_keys[bin][i] = key;
			_values[bin][i] = value;
			return;
		}
	}*/
	const __m128i zero_item = _mm_set1_epi32(0);
	const __m128i zero_match = _mm_cmpeq_epi32(zero_item, *((__m128i*) _keys[bin]));
	const int     zero_mask = _mm_movemask_epi8(zero_match);
	if (zero_mask != 0) {
		int tz = _tzcnt_u32(zero_mask);
		int empty_spot_in_bin = tz >> 2;
		_keys[bin][empty_spot_in_bin] = key;
		_values[bin][empty_spot_in_bin] = value;
		return;
	}

	int kicked_index = rand() & 0b11;
	K kicked_key = _keys[bin][kicked_index];
	V kicked_value = _values[bin][kicked_index];
	_keys[bin][kicked_index] = key;
	_values[bin][kicked_index] = value;

	uint64_t kicked_hash = xxh::xxhash3<64>(&kicked_key, sizeof(kicked_key), _seed1);
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
V HH_Cuckoo_no_FP_SIMD<K, V>::query(K key)
{
	uint64_t hash = xxh::xxhash3<64>(&key, sizeof(key), _seed1);
	uint64_t bin1 = hash & _mask;
	const __m128i item = _mm_set1_epi32((int)key);
	const __m128i match1 = _mm_cmpeq_epi32(item, *((__m128i*) _keys[bin1]));
	const int mask = _mm_movemask_epi8(match1);
	if (mask != 0) {
		//cout << tz1 << endl;

		int tz1 = _tzcnt_u32(mask);
		return _values[bin1][tz1 >> 2];
	}
	uint64_t bin2 = (hash >> 32) & _mask;
	const __m128i match2 = _mm_cmpeq_epi32(item, *((__m128i*) _keys[bin2]));
	const int mask2 = _mm_movemask_epi8(match2);
	if (mask2 != 0) {
		//cout << tz1 << endl;
		int tz2 = _tzcnt_u32(mask2);
		return _values[bin2][tz2 >> 2];
	}
	/*
	for (int i = bin_width - 1; i >= 0; --i) {
		if (_keys[bin1][i] == key) {
			return _values[bin1][i];
		}
	}
	uint64_t bin2 = (hash >> 32) & _mask;
	for (int i = bin_width - 1; i >= 0; --i) {
		if (_keys[bin2][i] == key) {
			return _values[bin2][i];
		}
	}*/
	return -1;
}

template<class K, class V>
void HH_Cuckoo_no_FP_SIMD<K, V>::try_remove(K key)
{
	uint64_t hash = xxh::xxhash3<64>(&key, sizeof(key), _seed1);
	uint64_t bin1 = hash & _mask;
	for (int i = bin_width - 1; i >= 0; --i) {
		if (_keys[bin1][i] == key) {
			_keys[bin1][i] = 0;
			return;
		}
	}
	uint64_t bin2 = (hash >> 32) & _mask;
	for (int i = bin_width - 1; i >= 0; --i) {
		if (_keys[bin2][i] == key) {
			_keys[bin2][i] = 0;
			return;
		}
	}
}

template<class K, class V>
void HH_Cuckoo_no_FP_SIMD<K, V>::test_correctness()
{
	std::unordered_map<K, V> true_map;

	for (int i = 0; i < (1 << 11); ++i) {
		uint32_t id = rand();
		uint32_t val = (rand() << 15) | rand();
		true_map[id] = val;

		insert(id, val);
		/*if (id == 12190) {
			cout << "12190 arrived with value" << val << endl;
		}
		if ((true_map[12190] != query(12190)) && (i >= 521)) {
			cout << i << endl;
		}*/
	}
	for (auto it = true_map.begin(); it != true_map.end(); ++it) {
		if (it->second != query(it->first)) {
			cout << it->first << " " << it->second << " " << query(it->first) << endl;
		}
	}
}

template<class K, class V>
void HH_Cuckoo_no_FP_SIMD<K, V>::test_speed()
{
	auto start = chrono::steady_clock::now();
	for (int64_t i = 0; i < 100000; ++i)
	{
		for (int64_t j = 0; j < 1000; ++j) {
			insert(i, i + j);
		}
		for (int64_t j = 0; j < 1000; ++j) {
			try_remove(i);
		}
	}
	auto end = chrono::steady_clock::now();

	auto time = chrono::duration_cast<chrono::microseconds>(end - start).count();
	cout << "test_speed: Elapsed time in milliseconds : "
		<< time / 1000
		<< " ms" << endl;
}

#endif //CUCKOO_WL_H
