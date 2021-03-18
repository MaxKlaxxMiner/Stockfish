/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2021 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <cstring>   // For std::memset
#include <iostream>
#include <thread>

#include "bitboard.h"
#include "misc.h"
#include "thread.h"
#include "tt.h"
#include "uci.h"

namespace Stockfish {

TranspositionTable TT; // Our global transposition table

/// TTEntry::save() populates the TTEntry with a new node's data, possibly
/// overwriting an old position. Update is not atomic and can be racy.

void TTEntry::save(Key k, Value v, bool pv, Bound b, Depth d, Move m, Value ev) {

  // Preserve any existing move for the same position
  if (m || (uint16_t)k != key16)
      move16 = (uint16_t)m;

  // Overwrite less valuable entries (cheapest checks first)
  if (b == BOUND_EXACT
      || (uint16_t)k != key16
      || d - DEPTH_OFFSET > depth8 - 4)
  {
      assert(d > DEPTH_OFFSET);
      assert(d < 256 + DEPTH_OFFSET);

      key16    = (uint16_t)k;
      depth8   = (uint8_t)(d - DEPTH_OFFSET);
      pvBound8 = (uint8_t)(uint8_t(pv) << 2 | b);
      value16  = (int16_t)v;
      eval16   = (int16_t)ev;
  }
}


/// TranspositionTable::resize() sets the size of the transposition table,
/// measured in megabytes.

void TranspositionTable::resize(size_t mbSize) {

  Threads.main()->wait_for_search_finished();

  aligned_large_pages_free(table);

  entryCount = mbSize * 1024 * 1024 / sizeof(TTEntry);

  table = static_cast<TTEntry*>(aligned_large_pages_alloc(entryCount * sizeof(TTEntry)));
  if (!table)
  {
      std::cerr << "Failed to allocate " << mbSize
                << "MB for transposition table." << std::endl;
      exit(EXIT_FAILURE);
  }

  clear();
}


/// TranspositionTable::clear() initializes the entire transposition table to zero,
//  in a multi-threaded way.

void TranspositionTable::clear() {

  std::vector<std::thread> threads;

  for (size_t idx = 0; idx < Options["Threads"]; ++idx)
  {
      threads.emplace_back([this, idx]() {

          // Thread binding gives faster search on systems with a first-touch policy
          if (Options["Threads"] > 8)
              WinProcGroup::bindThisThread(idx);

          // Each thread will zero its part of the hash table
          const size_t stride = size_t(entryCount / Options["Threads"]),
                       start  = size_t(stride * idx),
                       len    = idx != Options["Threads"] - 1 ?
                                stride : entryCount - start;

          std::memset(&table[start], 0, len * sizeof(TTEntry));
      });
  }

  for (std::thread& th : threads)
      th.join();
}


/// TranspositionTable::probe() looks up the current position in the transposition
/// table. It returns true and a pointer to the TTEntry if the position is found.
/// Otherwise, it returns false and a pointer to an empty TTEntry to be replaced later.

TTEntry* TranspositionTable::probe(const Key key, bool& found) const {

  TTEntry* const tte = get_entry(key);

  if (tte->key16 == (uint16_t)key)
  {
    return found = (bool)tte->depth8, tte;
  }

  return found = false, tte;
}


/// TranspositionTable::hashfull() returns an approximation of the hashtable
/// occupation during a search. The hash is x permill full, as per UCI protocol.

int TranspositionTable::hashfull() const {

  int cnt = 0;
  for (int i = 0; i < 1000; ++i)
    cnt += (bool)table[i].depth8;

  return cnt;
}

} // namespace Stockfish
