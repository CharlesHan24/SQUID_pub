/*#include "Cuckoo.h"

#include <chrono>

#include "xxhash.h"

#include <unordered_map>
#include <iostream>

using namespace std;



Cuckoo::Cuckoo(int seed1, int seed2, int seed3) {
	_seed1 = seed1;
	_seed2 = seed2;
	_seed3 = seed3;
	_mask = 0x3FF;
	srand(42);
}

Cuckoo::~Cuckoo() {

}

void Cuckoo::insert(uint32_t key, uint32_t value)
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
		uint32_t kicked_key = _keys[bin1][kicked_index];
		uint32_t kicked_value = _values[bin1][kicked_index];

		_keys[bin1][kicked_index] = key;
		_values[bin1][kicked_index] = value;
		_fingerprints[bin1][kicked_index] = fp;
		//insert(kicked_key, kicked_value);
		move_to_bin(kicked_key, kicked_value, kicked_fp, bin1 ^ (xxh::xxhash3<64>(&kicked_fp, sizeof(kicked_fp), _seed3)) & _mask);
	}
	else {
		int kicked_index = (coinFlip >> 1) & 0b11;
		uint32_t kicked_key = _keys[bin2][kicked_index];
		uint32_t kicked_value = _values[bin2][kicked_index];

		uint16_t kicked_fp = _fingerprints[bin2][kicked_index];
		_keys[bin2][kicked_index] = key;
		_values[bin2][kicked_index] = value;
		_fingerprints[bin2][kicked_index] = fp;
		move_to_bin(kicked_key, kicked_value, kicked_fp, bin2 ^ (xxh::xxhash3<64>(&kicked_fp, sizeof(kicked_fp), _seed3)) & _mask);
		//insert(kicked_key, kicked_value);
	}
}

void Cuckoo::move_to_bin(uint32_t key, uint32_t value, uint16_t fp, int bin)
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
	uint32_t kicked_key = _keys[bin][kicked_index];
	uint32_t kicked_value = _values[bin][kicked_index];
	_keys[bin][kicked_index] = key;
	_values[bin][kicked_index] = value;
	_fingerprints[bin][kicked_index] = fp;
	move_to_bin(kicked_key, kicked_value, kicked_fp, bin ^ (xxh::xxhash3<64>(&kicked_fp, sizeof(kicked_fp), _seed3)) & _mask);
}

uint32_t Cuckoo::query(uint32_t key)
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

void Cuckoo::try_remove(uint32_t key)
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


void Cuckoo::test_correctness()
{
	std::unordered_map<uint32_t, uint32_t> true_map;

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
		}
	}
	for (auto it = true_map.begin(); it != true_map.end(); ++it) {
		if (it->second != query(it->first)) {
			cout << it->first << " " << it->second << " " << query(it->first) << endl;
		}
	}
}

void Cuckoo::test_speed()
{
	auto start = chrono::steady_clock::now();
	for (int64_t i = 0; i < 10000; ++i)
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
*/