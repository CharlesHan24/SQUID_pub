#pragma once
#include <cstdint>
#ifndef CUCKOO_H
#define CUCKOO_H

#include <emmintrin.h>
#include "xxhash.h"

using namespace std;

constexpr int bin_width = 4;
constexpr int nr_bins = 1 << 20;
constexpr int nr_bins_over_two = nr_bins >> 1;


template<class K, class V>
class Cuckoo_FP {
private:
	uint16_t _fingerprints[nr_bins][bin_width] = {};
	K _keys[nr_bins][bin_width] = {};
	V _values[nr_bins][bin_width] = {};
	int _seed1;
	int _seed2;
	int _seed3;

	int _mask;

	void move_to_bin(K key, V value, uint16_t fp, int bin);
	
	//xxh::xxhash3<64>(str, FT_SIZE, seeds[0]);
public:
	void test_correctness();
	void test_speed();
	Cuckoo_FP(int seed1, int seed2, int seed3);
	~Cuckoo_FP();
	void insert(K key, V value);
	V query(K key);
	void try_remove(K key);
};


template<class K, class V>
Cuckoo_FP<K, V>::Cuckoo_FP(int seed1, int seed2, int seed3) {
	_seed1 = seed1;
	_seed2 = seed2;
	_seed3 = seed3;
	_mask = nr_bins - 1;
	srand(42);
}

template<class K, class V>
Cuckoo_FP<K, V>::~Cuckoo_FP() {

}

template<class K, class V>
void Cuckoo_FP<K, V>::insert(K key, V value)
{
	uint64_t hash = xxh::xxhash3<64>(&key, sizeof(key), _seed1);
	uint16_t fp = hash & 0xFFFF;
	uint64_t i = 1;
	while (fp == 0) {
		fp = xxh::xxhash3<64>(&key, sizeof(key), _seed1 + i) & 0xFFFF;
		++i;
	}
	uint16_t bin1 = (hash >> 16) & _mask;
	int empty_spot_in_bin1 = -1;
	int empty_spot_in_bin2 = -1;
	bool fp_found_in_bin1 = false;
	bool fp_found_in_bin2 = false;
	//for (int i = 0; i < bin_width; ++i) {
	for (int i = bin_width - 1; i >= 0; --i) {
		if (_fingerprints[bin1][i] == fp) {
			fp_found_in_bin1 = true;
			if (_keys[bin1][i] == key) {
				_values[bin1][i] = value;
				return;
			}
		}
		if (_fingerprints[bin1][i] == 0) {
			empty_spot_in_bin1 = i;
		}
	}
	uint16_t bin2 = bin1 ^ ((xxh::xxhash3<64>(&fp, sizeof(fp), _seed3)) & _mask);
	for (int i = bin_width - 1; i >= 0; --i) {
		if (_fingerprints[bin2][i] == fp) {
			fp_found_in_bin2 = true;
			if (_keys[bin2][i] == key) {
				_values[bin2][i] = value;
				return;
			}
		}
		if (_fingerprints[bin2][i] == 0) {
			empty_spot_in_bin2 = i;
		}
	}
	if (empty_spot_in_bin1 != -1) {
		_keys[bin1][empty_spot_in_bin1] = key;
		_values[bin1][empty_spot_in_bin1] = value;
		_fingerprints[bin1][empty_spot_in_bin1] = fp;
		return;
	}
	if (empty_spot_in_bin2 != -1) {
		_keys[bin2][empty_spot_in_bin2] = key;
		_values[bin2][empty_spot_in_bin2] = value;
		_fingerprints[bin2][empty_spot_in_bin2] = fp;
		return;
	}
	int coinFlip = rand();
	if (
		((coinFlip & 0b1) && (!fp_found_in_bin1)) ||
		((coinFlip & 0b0) && (fp_found_in_bin2))
		) {
		int kicked_index = (coinFlip >> 1) & 0b11;
		uint16_t kicked_fp = _fingerprints[bin1][kicked_index];
		K kicked_key = _keys[bin1][kicked_index];
		V kicked_value = _values[bin1][kicked_index];

		_keys[bin1][kicked_index] = key;
		_values[bin1][kicked_index] = value;
		_fingerprints[bin1][kicked_index] = fp;
		//insert(kicked_key, kicked_value);
		move_to_bin(kicked_key, kicked_value, kicked_fp, bin1 ^ (xxh::xxhash3<64>(&kicked_fp, sizeof(kicked_fp), _seed3)) & _mask);
	}
	else {
		int kicked_index = (coinFlip >> 1) & 0b11;
		K kicked_key = _keys[bin2][kicked_index];
		V kicked_value = _values[bin2][kicked_index];

		uint16_t kicked_fp = _fingerprints[bin2][kicked_index];
		_keys[bin2][kicked_index] = key;
		_values[bin2][kicked_index] = value;
		_fingerprints[bin2][kicked_index] = fp;
		move_to_bin(kicked_key, kicked_value, kicked_fp, bin2 ^ (xxh::xxhash3<64>(&kicked_fp, sizeof(kicked_fp), _seed3)) & _mask);
		//insert(kicked_key, kicked_value);
	}
}

template<class K, class V>
void Cuckoo_FP<K, V>::move_to_bin(K key, V value, uint16_t fp, int bin)
{
	//cout << "move_to_bin" << endl;
	int fp_found_in_bin = -1;
	for (int i = bin_width - 1; i >= 0; --i) {
		//for (int i = bin_width-1; i >= 0; --i) {
		if (_fingerprints[bin][i] == fp) {
			fp_found_in_bin = i;
		}
		if (_fingerprints[bin][i] == 0) {
			_keys[bin][i] = key;
			_values[bin][i] = value;
			_fingerprints[bin][i] = fp;
			return;
		}
	}
	int kicked_index = fp_found_in_bin;
	uint16_t kicked_fp = fp;
	if (fp_found_in_bin == -1) {
		kicked_index = rand() & 0b11;
		kicked_fp = _fingerprints[bin][kicked_index];
	}
	K kicked_key = _keys[bin][kicked_index];
	V kicked_value = _values[bin][kicked_index];
	_keys[bin][kicked_index] = key;
	_values[bin][kicked_index] = value;
	_fingerprints[bin][kicked_index] = fp;
	move_to_bin(kicked_key, kicked_value, kicked_fp, bin ^ (xxh::xxhash3<64>(&kicked_fp, sizeof(kicked_fp), _seed3)) & _mask);
}

template<class K, class V>
V Cuckoo_FP<K, V>::query(K key)
{
	uint64_t hash = xxh::xxhash3<64>(&key, sizeof(key), _seed1);
	uint16_t fp = hash & 0xFFFF;
	int i = 1;
	while (fp == 0) {
		fp = xxh::xxhash3<64>(&key, sizeof(key), _seed1 + i) & 0xFFFF;
		++i;
	}
	uint16_t bin1 = (hash >> 16) & _mask;
	for (int i = bin_width - 1; i >= 0; --i) {
		if (_fingerprints[bin1][i] == fp) {
			if (_keys[bin1][i] == key) {
				return _values[bin1][i];
			}
		}
	}
	uint16_t bin2 = bin1 ^ (xxh::xxhash3<64>(&fp, sizeof(fp), _seed3) & _mask);
	for (int i = bin_width - 1; i >= 0; --i) {
		if (_fingerprints[bin2][i] == fp) {
			if (_keys[bin2][i] == key) {
				return _values[bin2][i];
			}
		}
	}
	return -1;
}

template<class K, class V>
void Cuckoo_FP<K, V>::try_remove(K key)
{
	uint64_t hash = xxh::xxhash3<64>(&key, sizeof(key), _seed1);
	uint16_t fp = hash & 0xFFFF;
	int i = 1;
	while (fp == 0) {
		fp = xxh::xxhash3<64>(&key, sizeof(key), _seed1 + i) & 0xFFFF;
		++i;
	}
	uint16_t bin1 = (hash >> 16) & _mask;
	for (int i = bin_width - 1; i >= 0; --i) {
		if (_fingerprints[bin1][i] == fp) {
			if (_keys[bin1][i] == key) {
				_fingerprints[bin1][i] = 0;
				return;
			}
		}
	}
	uint16_t bin2 = bin1 ^ (xxh::xxhash3<64>(&fp, sizeof(fp), _seed3) & _mask);
	for (int i = bin_width - 1; i >= 0; --i) {
		if (_fingerprints[bin2][i] == fp) {
			if (_keys[bin2][i] == key) {
				_fingerprints[bin2][i] = 0;
				return;
			}
		}
	}
}

template<class K, class V>
void Cuckoo_FP<K, V>::test_correctness()
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
void Cuckoo_FP<K, V>::test_speed()
{
	auto start = chrono::steady_clock::now();
	for (int64_t i = 0; i < 10; ++i)
	{
		for (int64_t j = 0; j < 3.6*nr_bins; ++j) {
			insert(j, i + j);
		}
		for (int64_t j = 0; j < 3.6*nr_bins; ++j) {
			try_remove(j);
		}
	}
	auto end = chrono::steady_clock::now();

	auto time = chrono::duration_cast<chrono::microseconds>(end - start).count();
	cout << "test_speed: Elapsed time in milliseconds : "
		<< time / 1000
		<< " ms" << endl;
}


////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

template<class K, class V>
class Cuckoo_no_FP {
private:
	K _keys[nr_bins][bin_width] = {};
	V _values[nr_bins][bin_width] = {};
	int _seed1;
	int _seed2;
	int _seed3;

	int _mask;

	void move_to_bin(K key, V value, int bin);

	//xxh::xxhash3<64>(str, FT_SIZE, seeds[0]);
public:
	void test_correctness();
	void test_speed();
	Cuckoo_no_FP(int seed1, int seed2, int seed3);
	~Cuckoo_no_FP();
	void insert(K key, V value);
	V query(K key);
	void try_remove(K key);
};


template<class K, class V>
Cuckoo_no_FP<K, V>::Cuckoo_no_FP(int seed1, int seed2, int seed3) {
	_seed1 = seed1;
	_seed2 = seed2;
	_seed3 = seed3;
	_mask = nr_bins - 1;
	srand(42);
}

template<class K, class V>
Cuckoo_no_FP<K, V>::~Cuckoo_no_FP() {

}

template<class K, class V>
void Cuckoo_no_FP<K, V>::insert(K key, V value)
{
	uint64_t hash = xxh::xxhash3<64>(&key, sizeof(key), _seed1);
	uint64_t i = 1;
	uint64_t bin1 = hash & _mask;
	int empty_spot_in_bin1 = -1;
	int empty_spot_in_bin2 = -1;
	//for (int i = 0; i < bin_width; ++i) {
	for (int i = bin_width - 1; i >= 0; --i) {
		if (_keys[bin1][i] == key) {
			_values[bin1][i] = value;
			return;
		}
		if (_keys[bin1][i] == 0) {
			empty_spot_in_bin1 = i;
		}
	}
	uint64_t bin2 = (hash >> 32) & _mask;
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
void Cuckoo_no_FP<K, V>::move_to_bin(K key, V value, int bin)
{
	//cout << "move_to_bin" << endl;
	for (int i = bin_width - 1; i >= 0; --i) {
		if (_keys[bin][i] == 0) {
			_keys[bin][i] = key;
			_values[bin][i] = value;
			return;
		}
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
V Cuckoo_no_FP<K, V>::query(K key)
{
	uint64_t hash = xxh::xxhash3<64>(&key, sizeof(key), _seed1);
	uint64_t bin1 = hash & _mask;
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
	}
	return -1;
}

template<class K, class V>
void Cuckoo_no_FP<K, V>::try_remove(K key)
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
void Cuckoo_no_FP<K, V>::test_correctness()
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
void Cuckoo_no_FP<K, V>::test_speed()
{
	auto start = chrono::steady_clock::now();
	for (int64_t i = 0; i < 10; ++i)
	{
		for (int64_t j = 0; j < 3.6*nr_bins; ++j) {
			insert(j, i + j);
		}
		for (int64_t j = 0; j < 3.6*nr_bins; ++j) {
			try_remove(j);
		}
	}
	auto end = chrono::steady_clock::now();

	auto time = chrono::duration_cast<chrono::microseconds>(end - start).count();
	cout << "test_speed: Elapsed time in milliseconds : "
		<< time / 1000
		<< " ms" << endl;
}


////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

template<class K, class V>
class Cuckoo_FP_SIMD {
private:
	uint16_t _fingerprints[nr_bins][bin_width] = {};
	K _keys[nr_bins][bin_width] = {};
	V _values[nr_bins][bin_width] = {};
	int _seed1;
	int _seed2;
	int _seed3;

	int _mask;

	void move_to_bin(K key, V value, uint16_t fp, int bin);

	//xxh::xxhash3<64>(str, FT_SIZE, seeds[0]);
public:
	void test_correctness();
	void test_speed();
	Cuckoo_FP_SIMD(int seed1, int seed2, int seed3);
	~Cuckoo_FP_SIMD();
	void insert(K key, V value);
	V query(K key);
	void try_remove(K key);
};


template<class K, class V>
Cuckoo_FP_SIMD<K, V>::Cuckoo_FP_SIMD(int seed1, int seed2, int seed3) {
	_seed1 = seed1;
	_seed2 = seed2;
	_seed3 = seed3;
	_mask = nr_bins - 1;
	srand(42);
}

template<class K, class V>
Cuckoo_FP_SIMD<K, V>::~Cuckoo_FP_SIMD() {

}

template<class K, class V>
void Cuckoo_FP_SIMD<K, V>::insert(K key, V value)
{
	uint64_t hash = xxh::xxhash3<64>(&key, sizeof(key), _seed1);
	uint16_t fp = hash & 0xFFFF;
	uint64_t i = 1;
	while (fp == 0) {
		fp = xxh::xxhash3<64>(&key, sizeof(key), _seed1 + i) & 0xFFFF;
		++i;
	}
	uint16_t bin1 = (hash >> 16) & _mask;
	int empty_spot_in_bin1 = -1;
	int empty_spot_in_bin2 = -1;
	bool fp_found_in_bin1 = false;
	bool fp_found_in_bin2 = false;
	//for (int i = 0; i < bin_width; ++i) {
	for (int i = bin_width - 1; i >= 0; --i) {
		if (_fingerprints[bin1][i] == fp) {
			fp_found_in_bin1 = true;
			if (_keys[bin1][i] == key) {
				_values[bin1][i] = value;
				return;
			}
		}
		if (_fingerprints[bin1][i] == 0) {
			empty_spot_in_bin1 = i;
		}
	}
	uint16_t bin2 = bin1 ^ ((xxh::xxhash3<64>(&fp, sizeof(fp), _seed3)) & _mask);
	for (int i = bin_width - 1; i >= 0; --i) {
		if (_fingerprints[bin2][i] == fp) {
			fp_found_in_bin2 = true;
			if (_keys[bin2][i] == key) {
				_values[bin2][i] = value;
				return;
			}
		}
		if (_fingerprints[bin2][i] == 0) {
			empty_spot_in_bin2 = i;
		}
	}
	if (empty_spot_in_bin1 != -1) {
		_keys[bin1][empty_spot_in_bin1] = key;
		_values[bin1][empty_spot_in_bin1] = value;
		_fingerprints[bin1][empty_spot_in_bin1] = fp;
		return;
	}
	if (empty_spot_in_bin2 != -1) {
		_keys[bin2][empty_spot_in_bin2] = key;
		_values[bin2][empty_spot_in_bin2] = value;
		_fingerprints[bin2][empty_spot_in_bin2] = fp;
		return;
	}
	int coinFlip = rand();
	if (
		((coinFlip & 0b1) && (!fp_found_in_bin1)) ||
		((coinFlip & 0b0) && (fp_found_in_bin2))
		) {
		int kicked_index = (coinFlip >> 1) & 0b11;
		uint16_t kicked_fp = _fingerprints[bin1][kicked_index];
		K kicked_key = _keys[bin1][kicked_index];
		V kicked_value = _values[bin1][kicked_index];

		_keys[bin1][kicked_index] = key;
		_values[bin1][kicked_index] = value;
		_fingerprints[bin1][kicked_index] = fp;
		//insert(kicked_key, kicked_value);
		move_to_bin(kicked_key, kicked_value, kicked_fp, bin1 ^ (xxh::xxhash3<64>(&kicked_fp, sizeof(kicked_fp), _seed3)) & _mask);
	}
	else {
		int kicked_index = (coinFlip >> 1) & 0b11;
		K kicked_key = _keys[bin2][kicked_index];
		V kicked_value = _values[bin2][kicked_index];

		uint16_t kicked_fp = _fingerprints[bin2][kicked_index];
		_keys[bin2][kicked_index] = key;
		_values[bin2][kicked_index] = value;
		_fingerprints[bin2][kicked_index] = fp;
		move_to_bin(kicked_key, kicked_value, kicked_fp, bin2 ^ (xxh::xxhash3<64>(&kicked_fp, sizeof(kicked_fp), _seed3)) & _mask);
		//insert(kicked_key, kicked_value);
	}
}

template<class K, class V>
void Cuckoo_FP_SIMD<K, V>::move_to_bin(K key, V value, uint16_t fp, int bin)
{
	//cout << "move_to_bin" << endl;
	int fp_found_in_bin = -1;
	for (int i = bin_width - 1; i >= 0; --i) {
		//for (int i = bin_width-1; i >= 0; --i) {
		if (_fingerprints[bin][i] == fp) {
			fp_found_in_bin = i;
		}
		if (_fingerprints[bin][i] == 0) {
			_keys[bin][i] = key;
			_values[bin][i] = value;
			_fingerprints[bin][i] = fp;
			return;
		}
	}
	int kicked_index = fp_found_in_bin;
	uint16_t kicked_fp = fp;
	if (fp_found_in_bin == -1) {
		kicked_index = rand() & 0b11;
		kicked_fp = _fingerprints[bin][kicked_index];
	}
	K kicked_key = _keys[bin][kicked_index];
	V kicked_value = _values[bin][kicked_index];
	_keys[bin][kicked_index] = key;
	_values[bin][kicked_index] = value;
	_fingerprints[bin][kicked_index] = fp;
	move_to_bin(kicked_key, kicked_value, kicked_fp, bin ^ (xxh::xxhash3<64>(&kicked_fp, sizeof(kicked_fp), _seed3)) & _mask);
}

template<class K, class V>
V Cuckoo_FP_SIMD<K, V>::query(K key)
{
	uint64_t hash = xxh::xxhash3<64>(&key, sizeof(key), _seed1);
	uint16_t fp = hash & 0xFFFF;
	int i = 1;
	while (fp == 0) {
		fp = xxh::xxhash3<64>(&key, sizeof(key), _seed1 + i) & 0xFFFF;
		++i;
	}
	uint16_t bin1 = (hash >> 16) & _mask;
	for (int i = bin_width - 1; i >= 0; --i) {
		if (_fingerprints[bin1][i] == fp) {
			if (_keys[bin1][i] == key) {
				return _values[bin1][i];
			}
		}
		/*
		__m128i* keys_p = (__m128i*)_fingerprints[bin1];

		__m128i a_comp = _mm_cmpeq_epi16(fp, keys_p[0]);
		__m128i b_comp = _mm_cmpeq_epi16(fp, keys_p[1]);
		__m128i c_comp = _mm_cmpeq_epi16(fp, keys_p[2]);
		__m128i d_comp = _mm_cmpeq_epi16(fp, keys_p[3]);

		a_comp = _mm_packs_epi16(a_comp, b_comp);
		c_comp = _mm_packs_epi16(c_comp, d_comp);
		a_comp = _mm_packs_epi16(a_comp, c_comp);

		matched = _mm_movemask_epi8(a_comp);
		*/
	}
	uint16_t bin2 = bin1 ^ (xxh::xxhash3<64>(&fp, sizeof(fp), _seed3) & _mask);
	for (int i = bin_width - 1; i >= 0; --i) {
		if (_fingerprints[bin2][i] == fp) {
			if (_keys[bin2][i] == key) {
				return _values[bin2][i];
			}
		}
	}
	return -1;
}

template<class K, class V>
void Cuckoo_FP_SIMD<K, V>::try_remove(K key)
{
	uint64_t hash = xxh::xxhash3<64>(&key, sizeof(key), _seed1);
	uint16_t fp = hash & 0xFFFF;
	int i = 1;
	while (fp == 0) {
		fp = xxh::xxhash3<64>(&key, sizeof(key), _seed1 + i) & 0xFFFF;
		++i;
	}
	uint16_t bin1 = (hash >> 16) & _mask;
	for (int i = bin_width - 1; i >= 0; --i) {
		if (_fingerprints[bin1][i] == fp) {
			if (_keys[bin1][i] == key) {
				_fingerprints[bin1][i] = 0;
				return;
			}
		}
	}
	uint16_t bin2 = bin1 ^ (xxh::xxhash3<64>(&fp, sizeof(fp), _seed3) & _mask);
	for (int i = bin_width - 1; i >= 0; --i) {
		if (_fingerprints[bin2][i] == fp) {
			if (_keys[bin2][i] == key) {
				_fingerprints[bin2][i] = 0;
				return;
			}
		}
	}
}

template<class K, class V>
void Cuckoo_FP_SIMD<K, V>::test_correctness()
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
void Cuckoo_FP_SIMD<K, V>::test_speed()
{
	auto start = chrono::steady_clock::now();
	for (int64_t i = 0; i < 10; ++i)
	{
		for (int64_t j = 0; j < 3.6*nr_bins; ++j) {
			insert(j, i + j);
		}
		for (int64_t j = 0; j < 3.6*nr_bins; ++j) {
			try_remove(j);
		}
	}
	auto end = chrono::steady_clock::now();

	auto time = chrono::duration_cast<chrono::microseconds>(end - start).count();
	cout << "test_speed: Elapsed time in milliseconds : "
		<< time / 1000
		<< " ms" << endl;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////


template<class K, class V>
class Cuckoo_no_FP_SIMD {
private:
	K _keys[nr_bins][bin_width] = {};
	V _values[nr_bins][bin_width] = {};
	int _seed1;
	int _seed2;
	int _seed3;

	int _mask;

	void move_to_bin(K key, V value, int bin);

	//xxh::xxhash3<64>(str, FT_SIZE, seeds[0]);
public:
	void test_correctness();
	void test_speed();
	Cuckoo_no_FP_SIMD(int seed1, int seed2, int seed3);
	~Cuckoo_no_FP_SIMD();
	void insert(K key, V value);
	V query(K key);
	void try_remove(K key);
};


template<class K, class V>
Cuckoo_no_FP_SIMD<K, V>::Cuckoo_no_FP_SIMD(int seed1, int seed2, int seed3) {
	_seed1 = seed1;
	_seed2 = seed2;
	_seed3 = seed3;
	_mask = nr_bins - 1;
	srand(42);
}

template<class K, class V>
Cuckoo_no_FP_SIMD<K, V>::~Cuckoo_no_FP_SIMD() {

}

template<class K, class V>
void Cuckoo_no_FP_SIMD<K, V>::insert(K key, V value)
{
	uint64_t hash = xxh::xxhash3<64>(&key, sizeof(key), _seed1);
	uint64_t i = 1;
	uint64_t bin1 = hash & _mask;
	int empty_spot_in_bin1 = -1;
	int empty_spot_in_bin2 = -1;
	//for (int i = 0; i < bin_width; ++i) {
	/*
	for (int i = bin_width - 1; i >= 0; --i) {
		if (_keys[bin1][i] == key) {
			_values[bin1][i] = value;
			return;
		}
		if (_keys[bin1][i] == 0) {
			empty_spot_in_bin1 = i;
		}
	
	}*/	
	const __m128i zero_item = _mm_set1_epi32(0);
	const __m128i item   = _mm_set1_epi32((int)key);

	const __m128i match1 = _mm_cmpeq_epi32(item, *((__m128i*) _keys[bin1]));
	const int     mask1  = _mm_movemask_epi8(match1);
	if (mask1 != 0) {
		int tz1 = _tzcnt_u32(mask1);
		_values[bin1][tz1 >> 2] = value;
		return;
	}
	const __m128i zero_match1 = _mm_cmpeq_epi32(zero_item, *((__m128i*) _keys[bin1]));
	const int     zero_mask1 = _mm_movemask_epi8(zero_match1);
	if (zero_mask1 != 0) {
		int tz1 = _tzcnt_u32(zero_mask1);
		empty_spot_in_bin1 = tz1 >> 2;
	}
	
	
	uint64_t bin2 = (hash >> 32) & _mask;
	
	const __m128i match2 = _mm_cmpeq_epi32(item, *((__m128i*) _keys[bin2]));
	const int     mask2 = _mm_movemask_epi8(match2);
	if (mask2 != 0) {
		int tz2 = _tzcnt_u32(mask2);
		_values[bin2][tz2 >> 2] = value;
		return;
	}
	const __m128i zero_match2 = _mm_cmpeq_epi32(zero_item, *((__m128i*) _keys[bin2]));
	const int     zero_mask2 = _mm_movemask_epi8(zero_match2);
	if (zero_mask2 != 0) {
		int tz2 = _tzcnt_u32(zero_mask2);
		empty_spot_in_bin2 = tz2 >> 2;
	}
	
	/*
	for (int i = bin_width - 1; i >= 0; --i) {
		if (_keys[bin2][i] == key) {
			_values[bin2][i] = value;
			return;
		}
		if (_keys[bin2][i] == 0) {
			empty_spot_in_bin2 = i;
		}
	}*/
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
void Cuckoo_no_FP_SIMD<K, V>::move_to_bin(K key, V value, int bin)
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
V Cuckoo_no_FP_SIMD<K, V>::query(K key)
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
void Cuckoo_no_FP_SIMD<K, V>::try_remove(K key)
{
	uint64_t hash = xxh::xxhash3<64>(&key, sizeof(key), _seed1);
	uint64_t bin1 = hash & _mask;

	const __m128i item = _mm_set1_epi32((int)key);

	const __m128i match1 = _mm_cmpeq_epi32(item, *((__m128i*) _keys[bin1]));
	const int     mask1 = _mm_movemask_epi8(match1);
	if (mask1 != 0) {
		int tz1 = _tzcnt_u32(mask1);
		_keys[bin1][tz1 >> 2] = 0;
		return;
	}
	uint64_t bin2 = (hash >> 32) & _mask;
	const __m128i match2 = _mm_cmpeq_epi32(item, *((__m128i*) _keys[bin2]));
	const int mask2 = _mm_movemask_epi8(match2);
	if (mask2 != 0) {
		//cout << tz1 << endl;
		int tz2 = _tzcnt_u32(mask2);
		_keys[bin2][tz2 >> 2] = 0;
	}

	/*
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
	}*/
}

template<class K, class V>
void Cuckoo_no_FP_SIMD<K, V>::test_correctness()
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
void Cuckoo_no_FP_SIMD<K, V>::test_speed()
{
	auto start = chrono::steady_clock::now();
	for (int64_t i = 0; i < 10; ++i)
	{
		for (int64_t j = 0; j < 3.6*nr_bins; ++j) {
			insert(j, i + j);
		}
		for (int64_t j = 0; j < 3.6*nr_bins; ++j) {
			try_remove(j);
		}
	}
	auto end = chrono::steady_clock::now();

	auto time = chrono::duration_cast<chrono::microseconds>(end - start).count();
	cout << "test_speed: Elapsed time in milliseconds : "
		<< time / 1000
		<< " ms" << endl;
}

/////////////////////////////////
template<class K, class V>
class Cuckoo_no_FP_SIMD_256 {
private:
	K _keys[nr_bins_over_two][8] = {};
	V _values[nr_bins_over_two][8] = {};
	int _seed1;
	int _seed2;
	int _seed3;

	int _mask;

	void move_to_bin(K key, V value, int bin);

	//xxh::xxhash3<64>(str, FT_SIZE, seeds[0]);
public:
	void test_correctness();
	void test_speed();
	Cuckoo_no_FP_SIMD_256(int seed1, int seed2, int seed3);
	~Cuckoo_no_FP_SIMD_256();
	void insert(K key, V value);
	V query(K key);
	void try_remove(K key);
};


template<class K, class V>
Cuckoo_no_FP_SIMD_256<K, V>::Cuckoo_no_FP_SIMD_256(int seed1, int seed2, int seed3) {
	_seed1 = seed1;
	_seed2 = seed2;
	_seed3 = seed3;
	_mask = nr_bins_over_two-1;
	srand(42);
}

template<class K, class V>
Cuckoo_no_FP_SIMD_256<K, V>::~Cuckoo_no_FP_SIMD_256() {

}

template<class K, class V>
void Cuckoo_no_FP_SIMD_256<K, V>::insert(K key, V value)
{
	uint64_t hash = xxh::xxhash3<64>(&key, sizeof(key), _seed1);
	uint64_t i = 1;
	uint64_t bin1 = hash & _mask;
	int empty_spot_in_bin1 = -1;
	int empty_spot_in_bin2 = -1;

	const __m256i zero_item = _mm256_set1_epi32(0);
	const __m256i item = _mm256_set1_epi32((int)key);

	const __m256i match1 = _mm256_cmpeq_epi32(item, *((__m256i*) _keys[bin1]));
	const int     mask1 = _mm256_movemask_epi8(match1);
	if (mask1 != 0) {
		int tz1 = _tzcnt_u32(mask1);
		_values[bin1][tz1 >> 2] = value;
		return;
	}
	const __m256i zero_match1 = _mm256_cmpeq_epi32(zero_item, *((__m256i*) _keys[bin1]));
	const int     zero_mask1 = _mm256_movemask_epi8(zero_match1);
	if (zero_mask1 != 0) {
		int tz1 = _tzcnt_u32(zero_mask1);
		empty_spot_in_bin1 = tz1 >> 2;
	}


	uint64_t bin2 = (hash >> 32) & _mask;

	const __m256i match2 = _mm256_cmpeq_epi32(item, *((__m256i*) _keys[bin2]));
	const int     mask2 = _mm256_movemask_epi8(match2);
	if (mask2 != 0) {
		int tz2 = _tzcnt_u32(mask2);
		_values[bin2][tz2 >> 2] = value;
		return;
	}
	const __m256i zero_match2 = _mm256_cmpeq_epi32(zero_item, *((__m256i*) _keys[bin2]));
	const int     zero_mask2 = _mm256_movemask_epi8(zero_match2);
	if (zero_mask2 != 0) {
		int tz2 = _tzcnt_u32(zero_mask2);
		empty_spot_in_bin2 = tz2 >> 2;
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
		int kicked_index = (coinFlip >> 1) & 0b111;
		kicked_key = _keys[bin1][kicked_index];
		kicked_value = _values[bin1][kicked_index];

		_keys[bin1][kicked_index] = key;
		_values[bin1][kicked_index] = value;
		kicked_from_bin = bin1;
		//insert(kicked_key, kicked_value);
	}
	else {
		int kicked_index = (coinFlip >> 1) & 0b111;
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
void Cuckoo_no_FP_SIMD_256<K, V>::move_to_bin(K key, V value, int bin)
{
	//cout << "move_to_bin" << endl;
	/*for (int i = 8 - 1; i >= 0; --i) {
		if (_keys[bin][i] == 0) {
			_keys[bin][i] = key;
			_values[bin][i] = value;
			return;
		}
	}*/
	const __m256i zero_item = _mm256_set1_epi32(0);
	const __m256i zero_match = _mm256_cmpeq_epi32(zero_item, *((__m256i*) _keys[bin]));
	const int     zero_mask = _mm256_movemask_epi8(zero_match);
	if (zero_mask != 0) {
		int tz = _tzcnt_u32(zero_mask);
		int empty_spot_in_bin = tz >> 2;
		_keys[bin][empty_spot_in_bin] = key;
		_values[bin][empty_spot_in_bin] = value;
		return;
	}

	int kicked_index = rand() & 0b111;
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
V Cuckoo_no_FP_SIMD_256<K, V>::query(K key)
{
	uint64_t hash = xxh::xxhash3<64>(&key, sizeof(key), _seed1);
	uint64_t bin1 = hash & _mask;
	const __m256i item = _mm256_set1_epi32((int)key);
	const __m256i match1 = _mm256_cmpeq_epi32(item, *((__m256i*) _keys[bin1]));
	const int mask = _mm256_movemask_epi8(match1);
	if (mask != 0) {
		//cout << tz1 << endl;

		int tz1 = _tzcnt_u32(mask);
		return _values[bin1][tz1 >> 2];
	}
	uint64_t bin2 = (hash >> 32) & _mask;
	const __m256i match2 = _mm256_cmpeq_epi32(item, *((__m256i*) _keys[bin2]));
	const int mask2 = _mm256_movemask_epi8(match2);
	if (mask2 != 0) {
		//cout << tz1 << endl;
		int tz2 = _tzcnt_u32(mask2);
		return _values[bin2][tz2 >> 2];
	}
	/*
	for (int i = 8 - 1; i >= 0; --i) {
		if (_keys[bin1][i] == key) {
			return _values[bin1][i];
		}
	}
	uint64_t bin2 = (hash >> 32) & _mask;
	for (int i = 8 - 1; i >= 0; --i) {
		if (_keys[bin2][i] == key) {
			return _values[bin2][i];
		}
	}*/
	return -1;
}

template<class K, class V>
void Cuckoo_no_FP_SIMD_256<K, V>::try_remove(K key)
{
	uint64_t hash = xxh::xxhash3<64>(&key, sizeof(key), _seed1);
	uint64_t bin1 = hash & _mask;

	const __m256i item = _mm256_set1_epi32((int)key);

	const __m256i match1 = _mm256_cmpeq_epi32(item, *((__m256i*) _keys[bin1]));
	const int     mask1 = _mm256_movemask_epi8(match1);
	if (mask1 != 0) {
		int tz1 = _tzcnt_u32(mask1);
		_keys[bin1][tz1 >> 2] = 0;
		return;
	}
	uint64_t bin2 = (hash >> 32) & _mask;
	const __m256i match2 = _mm256_cmpeq_epi32(item, *((__m256i*) _keys[bin2]));
	const int mask2 = _mm256_movemask_epi8(match2);
	if (mask2 != 0) {
		//cout << tz1 << endl;
		int tz2 = _tzcnt_u32(mask2);
		_keys[bin2][tz2 >> 2] = 0;
	}

	/*
	uint64_t bin1 = hash & _mask;
	for (int i = 8 - 1; i >= 0; --i) {
		if (_keys[bin1][i] == key) {
			_keys[bin1][i] = 0;
			return;
		}
	}
	uint64_t bin2 = (hash >> 32) & _mask;
	for (int i = 8 - 1; i >= 0; --i) {
		if (_keys[bin2][i] == key) {
			_keys[bin2][i] = 0;
			return;
		}
	}*/
}

template<class K, class V>
void Cuckoo_no_FP_SIMD_256<K, V>::test_correctness()
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
void Cuckoo_no_FP_SIMD_256<K, V>::test_speed()
{
	auto start = chrono::steady_clock::now();
	for (int64_t i = 0; i < 10; ++i)
	{
		for (int64_t j = 0; j < 3.6*nr_bins; ++j) {
			insert(j, i + j);
		}
		for (int64_t j = 0; j < 3.6*nr_bins; ++j) {
			try_remove(j);
		}
	}
	auto end = chrono::steady_clock::now();

	auto time = chrono::duration_cast<chrono::microseconds>(end - start).count();
	cout << "test_speed: Elapsed time in milliseconds : "
		<< time / 1000
		<< " ms" << endl;
}

/////////////////////////////////
template<class K, class V>
class Cuckoo_waterLevel_no_FP_SIMD_256 {
private:
	K _keys[nr_bins_over_two][8] = {};
	V _values[nr_bins_over_two][8] = {};

	V _water_level_plus_1;
	__m256i _wl_item;

	int _seed1;
	int _seed2;
	int _seed3;

	int _mask;

	void move_to_bin(K key, V value, int bin);

	//xxh::xxhash3<64>(str, FT_SIZE, seeds[0]);
public:
	void set_water_level(V wl);
	void test_correctness();
	void test_speed();
	Cuckoo_waterLevel_no_FP_SIMD_256(int seed1, int seed2, int seed3);
	~Cuckoo_waterLevel_no_FP_SIMD_256();
	void insert(K key, V value);
	V query(K key);
	void try_remove(K key);
};


template<class K, class V>
Cuckoo_waterLevel_no_FP_SIMD_256<K, V>::Cuckoo_waterLevel_no_FP_SIMD_256(int seed1, int seed2, int seed3) {
	_seed1 = seed1;
	_seed2 = seed2;
	_seed3 = seed3;
	_mask = nr_bins_over_two - 1;
	_water_level_plus_1 = 1;
	_wl_item = _mm256_set1_epi32(_water_level_plus_1);
	srand(42);
}

template<class K, class V>
Cuckoo_waterLevel_no_FP_SIMD_256<K, V>::~Cuckoo_waterLevel_no_FP_SIMD_256() {

}

template<class K, class V>
void Cuckoo_waterLevel_no_FP_SIMD_256<K, V>::insert(K key, V value)
{
	uint64_t hash = xxh::xxhash3<64>(&key, sizeof(key), _seed1);
	uint64_t i = 1;
	uint64_t bin1 = hash & _mask;
	int empty_spot_in_bin1 = -1;
	int empty_spot_in_bin2 = -1;

	//const __m256i wl_item = _mm256_set1_epi32(_water_level_plus_1);
	const __m256i item = _mm256_set1_epi32((int)key);

	const __m256i match1 = _mm256_cmpeq_epi32(item, *((__m256i*) _keys[bin1]));
	const int     mask1 = _mm256_movemask_epi8(match1);
	if (mask1 != 0) {
		int tz1 = _tzcnt_u32(mask1);
		_values[bin1][tz1 >> 2] = value;
		return;
	}
	//const __m256i wl_match1 = _mm256_cmple_epi32(*((__m256i*) _values[bin1]), wl_item);
	const __m256i wl_match1 = _mm256_cmpgt_epi32(_wl_item , *((__m256i*) _values[bin1]));
	const int     wl_mask1 = _mm256_movemask_epi8(wl_match1);

	if (wl_mask1 != 0) {
		int tz1 = _tzcnt_u32(wl_mask1);
		empty_spot_in_bin1 = tz1 >> 2;
	}


	uint64_t bin2 = (hash >> 32) & _mask;

	const __m256i match2 = _mm256_cmpeq_epi32(item, *((__m256i*) _keys[bin2]));
	const int     mask2 = _mm256_movemask_epi8(match2);
	if (mask2 != 0) {
		int tz2 = _tzcnt_u32(mask2);
		_values[bin2][tz2 >> 2] = value;
		return;
	}
	const __m256i wl_match2 = _mm256_cmpgt_epi32(_wl_item, *((__m256i*) _keys[bin2]));
	const int     wl_mask2 = _mm256_movemask_epi8(wl_match2);
	if (wl_mask2 != 0) {
		int tz2 = _tzcnt_u32(wl_mask2);
		empty_spot_in_bin2 = tz2 >> 2;
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
		int kicked_index = (coinFlip >> 1) & 0b111;
		kicked_key = _keys[bin1][kicked_index];
		kicked_value = _values[bin1][kicked_index];

		_keys[bin1][kicked_index] = key;
		_values[bin1][kicked_index] = value;
		kicked_from_bin = bin1;
		//insert(kicked_key, kicked_value);
	}
	else {
		int kicked_index = (coinFlip >> 1) & 0b111;
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
void Cuckoo_waterLevel_no_FP_SIMD_256<K, V>::move_to_bin(K key, V value, int bin)
{
	const __m256i wl_match = _mm256_cmpgt_epi32(_wl_item, *((__m256i*) _keys[bin]));
	const int     wl_mask = _mm256_movemask_epi8(wl_match);
	if (wl_mask != 0) {
		int tz = _tzcnt_u32(wl_mask);
		int empty_spot_in_bin = tz >> 2;
		_keys[bin][empty_spot_in_bin] = key;
		_values[bin][empty_spot_in_bin] = value;
		return;
	}

	int kicked_index = rand() & 0b111;
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
V Cuckoo_waterLevel_no_FP_SIMD_256<K, V>::query(K key)
{
	uint64_t hash = xxh::xxhash3<64>(&key, sizeof(key), _seed1);
	uint64_t bin1 = hash & _mask;
	const __m256i item = _mm256_set1_epi32((int)key);
	const __m256i match1 = _mm256_cmpeq_epi32(item, *((__m256i*) _keys[bin1]));
	const int mask = _mm256_movemask_epi8(match1);
	if (mask != 0) {
		int tz1 = _tzcnt_u32(mask);
		return _values[bin1][tz1 >> 2];
	}
	uint64_t bin2 = (hash >> 32) & _mask;
	const __m256i match2 = _mm256_cmpeq_epi32(item, *((__m256i*) _keys[bin2]));
	const int mask2 = _mm256_movemask_epi8(match2);
	if (mask2 != 0) {
		//cout << tz1 << endl;
		int tz2 = _tzcnt_u32(mask2);
		return _values[bin2][tz2 >> 2];
	}
	return 0;
}

template<class K, class V>
void Cuckoo_waterLevel_no_FP_SIMD_256<K, V>::try_remove(K key)
{
	uint64_t hash = xxh::xxhash3<64>(&key, sizeof(key), _seed1);
	uint64_t bin1 = hash & _mask;

	const __m256i item = _mm256_set1_epi32((int)key);

	const __m256i match1 = _mm256_cmpeq_epi32(item, *((__m256i*) _keys[bin1]));
	const int     mask1 = _mm256_movemask_epi8(match1);
	if (mask1 != 0) {
		int tz1 = _tzcnt_u32(mask1);
		_values[bin1][tz1 >> 2] = 0;
		return;
	}
	uint64_t bin2 = (hash >> 32) & _mask;
	const __m256i match2 = _mm256_cmpeq_epi32(item, *((__m256i*) _keys[bin2]));
	const int mask2 = _mm256_movemask_epi8(match2);
	if (mask2 != 0) {
		//cout << tz1 << endl;
		int tz2 = _tzcnt_u32(mask2);
		_values[bin2][tz2 >> 2] = 0;
	}
}

template<class K, class V>
inline void Cuckoo_waterLevel_no_FP_SIMD_256<K, V>::set_water_level(V wl)
{
	_water_level_plus_1 = wl + 1;
	_wl_item = _mm256_set1_epi32(_water_level_plus_1);
}

template<class K, class V>
void Cuckoo_waterLevel_no_FP_SIMD_256<K, V>::test_correctness()
{
	std::unordered_map<K, V> true_map;

	for (int i = 0; i < (1 << 11); ++i) {
		uint32_t id = rand();
		uint32_t val = (rand() << 15) | rand();
		true_map[id] = val;

		insert(id, val);
		try_remove(id);
		if (query(id) != 0) {
			cout << id << "was deleted!" << endl;
			exit(1);
		}
		insert(id, val);
	}
	for (auto it = true_map.begin(); it != true_map.end(); ++it) {
		if (it->second != query(it->first)) {
			cout << it->first << " " << it->second << " " << query(it->first) << endl;
			exit(1);
		}
	}
}

template<class K, class V>
void Cuckoo_waterLevel_no_FP_SIMD_256<K, V>::test_speed()
{
	auto start = chrono::steady_clock::now();
	for (int64_t i = 0; i < 10; ++i)
	{
		for (int64_t j = 0; j < 3.6 * nr_bins; ++j) {
			insert(j, i + j);
		}
		for (int64_t j = 0; j < 3.6 * nr_bins; ++j) {
			try_remove(j);
		}
	}
	auto end = chrono::steady_clock::now();

	auto time = chrono::duration_cast<chrono::microseconds>(end - start).count();
	cout << "test_speed: Elapsed time in milliseconds : "
		<< time / 1000
		<< " ms" << endl;
}


#endif //CUCKOO_H
