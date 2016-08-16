/*
This file is part of khmer, https://github.com/dib-lab/khmer/, and is
Copyright (C) 2015-2016, The Regents of the University of California.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.

    * Neither the name of the Michigan State University nor the names
      of its contributors may be used to endorse or promote products
      derived from this software without specific prior written
      permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
LICENSE (END)

Contact: khmer-project@idyll.org
*/
#include "khmer.hh"
#include "hashtable.hh"
#include "traversal.hh"
#include "symbols.hh"
#include "kmer_hash.hh"

#define DEBUG 1

using namespace std;

namespace khmer {


Traverser::Traverser(const Hashtable * ht) :
    KmerFactory(ht->ksize()), graph(ht)
{
    bitmask = 0;
    for (unsigned int i = 0; i < _ksize; i++) {
        bitmask = (bitmask << 2) | 3;
    }
    rc_left_shift = _ksize * 2 - 2;
}

Kmer Traverser::get_left(const Kmer& node, const char ch)
    const
{
    HashIntoType kmer_f, kmer_r;
    kmer_f = ((node.kmer_f) >> 2 | twobit_repr(ch) << rc_left_shift);
    kmer_r = (((node.kmer_r) << 2) & bitmask) | (twobit_comp(ch));
    return build_kmer(kmer_f, kmer_r);
}


Kmer Traverser::get_right(const Kmer& node, const char ch)
    const
{
    HashIntoType kmer_f, kmer_r;
    kmer_f = (((node.kmer_f) << 2) & bitmask) | (twobit_repr(ch));
    kmer_r = ((node.kmer_r) >> 2) | (twobit_comp(ch) << rc_left_shift);
    return build_kmer(kmer_f, kmer_r);
}


unsigned int Traverser::traverse_left(Kmer& node,
                                      KmerQueue & node_q,
                                      std::function<bool (Kmer&)> filter,
                                      unsigned short max_neighbors)
    const
{
    unsigned int found = 0;
    char * base = alphabets::DNA_SIMPLE;

    while(*base != '\0') {
        Kmer prev_node = get_left(node, *base);
        if (graph->get_count(prev_node) && (!filter || filter(prev_node))) {
            node_q.push(prev_node);
            ++found;
            if (found > max_neighbors) {
                return found;
            }
        }
        ++base;
    }

    return found;
}

unsigned int Traverser::traverse_right(Kmer& node,
                                       KmerQueue & node_q,
                                       std::function<bool (Kmer&)> filter,
                                       unsigned short max_neighbors)
    const
{
    unsigned int found = 0;
    char * base = alphabets::DNA_SIMPLE;

    while(*base != '\0') {
        Kmer next_node = get_right(node, *base);
        if (graph->get_count(next_node) && (!filter || filter(next_node))) {
            node_q.push(next_node);
            ++found;
            if (found > max_neighbors) {
                return found;
            }
        }
        ++base;
    }

    return found;
}

unsigned int Traverser::degree_left(const Kmer& node)
    const
{
    unsigned int degree = 0;
    char * base = alphabets::DNA_SIMPLE;

    while(*base != '\0') {
        Kmer prev_node = get_left(node, *base);
        if (graph->get_count(prev_node)) {
            ++degree;
        }
        ++base;
    }

    return degree;
}

unsigned int Traverser::degree_right(const Kmer& node)
    const
{
    unsigned int degree = 0;
    char * base = alphabets::DNA_SIMPLE;

    while(*base != '\0') {
        Kmer next_node = get_right(node, *base);
        if (graph->get_count(next_node)) {
            ++degree;
        }
        ++base;
    }

    return degree;
}

unsigned int Traverser::degree(const Kmer& node)
    const
{
    return degree_right(node) + degree_left(node);
}



template<bool direction>
AssemblerTraverser<direction>::AssemblerTraverser(const Hashtable * ht,
                                 Kmer start_kmer,
                                 KmerFilterList filters) :
    Traverser(ht), filters(filters)
{
    cursor = start_kmer;
}

template<>
Kmer AssemblerTraverser<LEFT>::get_neighbor(Kmer& node,
                                            const char symbol) {
    return get_left(node, symbol);
}

template<>
Kmer AssemblerTraverser<RIGHT>::get_neighbor(Kmer& node,
                                             const char symbol) {
    return get_right(node, symbol);
}

template<>
unsigned int AssemblerTraverser<LEFT>::cursor_degree()
    const
{
    return degree_left(cursor);
}

template<>
unsigned int AssemblerTraverser<RIGHT>::cursor_degree()
    const
{
    return degree_right(cursor);
}

template <>
std::string AssemblerTraverser<RIGHT>::join_contigs(std::string& contig_a,
                                                           std::string& contig_b)
    const
{
    return contig_a + contig_b.substr(_ksize);
}

template <>
std::string AssemblerTraverser<LEFT>::join_contigs(std::string& contig_a,
                                                         std::string& contig_b)
    const
{
    return contig_b + contig_a.substr(_ksize);
}

template<bool direction>
char AssemblerTraverser<direction>::next_symbol()
{
    char * symbol_ptr = alphabets::DNA_SIMPLE;
    char base;
    short found = 0;
    Kmer neighbor;
    Kmer cursor_next;

    while(*symbol_ptr != '\0') {
        neighbor = get_neighbor(cursor, *symbol_ptr);

        if (graph->get_count(neighbor) &&
            !apply_kmer_filters(neighbor, filters)) {

            found++;
            if (found > 1) {
                return '\0';
            }
            base = *symbol_ptr;
            cursor_next = neighbor;
        }
        symbol_ptr++;
    }

    if (!found) {
        return '\0';
    } else {
        cursor = cursor_next;
        return base;
    }
}


template<bool direction>
bool AssemblerTraverser<direction>::set_cursor(Kmer& node)
{
    if(!apply_kmer_filters(node, filters)) {
        cursor = node;
        return true;
    }
    return false;
}

template<bool direction>
void AssemblerTraverser<direction>::push_filter(KmerFilter filter)
{
    filters.push_back(filter);
}

template<bool direction>
KmerFilter AssemblerTraverser<direction>::pop_filter()
{
    KmerFilter back = filters.back();
    filters.pop_back();
    return back;
}

template<bool direction>
NonLoopingAT<direction>::NonLoopingAT(const Hashtable * ht,
                                      Kmer start_kmer,
                                      KmerFilterList filters,
                                      const SeenSet * visited) :
    AssemblerTraverser<direction>(ht, start_kmer, filters), visited(visited)
{
    AssemblerTraverser<direction>::push_filter(get_visited_filter(visited));
}

template<bool direction>
char NonLoopingAT<direction>::next_symbol()
{
    visited->insert(this->cursor);
    #if DEBUG
    std::cout << "next_symbol; visited " << visited->size() << std::endl;
    #endif
    return AssemblerTraverser<direction>::next_symbol();
}


template class AssemblerTraverser<RIGHT>;
template class AssemblerTraverser<LEFT>;
template class NonLoopingAT<RIGHT>;
template class NonLoopingAT<LEFT>;


} // namespace khmer
