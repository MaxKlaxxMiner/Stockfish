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

#ifndef TT_H_INCLUDED
#define TT_H_INCLUDED

#include "misc.h"
#include "types.h"

namespace Stockfish {

/// TTEntry struct is the 16 bytes transposition table entry, defined as below:
///
/// key        32 bit
/// depth       8 bit
/// generation  5 bit
/// pv node     1 bit
/// bound type  2 bit
/// move       16 bit
/// value      16 bit
/// eval value 16 bit
/// unused     32 bit

struct TTEntry {

  Move  move()  const { return (Move )move16; }
  Value value() const { return (Value)value16; }
  Value eval()  const { return (Value)eval16; }
  Depth depth() const { return (Depth)depth8 + DEPTH_OFFSET; }
  bool is_pv()  const { return (bool)(pvBound8 & 0x4); }
  Bound bound() const { return (Bound)(pvBound8 & 0x3); }
  void save(Key k, Value v, bool pv, Bound b, Depth d, Move m, Value ev);

private:
  friend class TranspositionTable;

  uint32_t key32;
  uint8_t  depth8;
  uint8_t  pvBound8;
  uint16_t move16;
  int16_t  value16;
  int16_t  eval16;
  uint32_t unused;
};


/// A TranspositionTable is an array of TTEntry, of size entryCount.
/// Each non-empty TTEntry contains information on exactly one position. 

class TranspositionTable {

public:
 ~TranspositionTable() { aligned_large_pages_free(table); }
  TTEntry* probe(const Key key, bool& found) const;
  int hashfull() const;
  void resize(size_t mbSize);
  void clear();

  TTEntry* get_entry(const Key key) const {
    return &table[mul_hi64(key, entryCount)];
  }

private:
  friend struct TTEntry;

  size_t entryCount;
  TTEntry* table;
};

extern TranspositionTable TT;

} // namespace Stockfish

#endif // #ifndef TT_H_INCLUDED
