/*
BSD 3-Clause License

Copyright (c) 2023, tomgrean

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef SRC_TBL_H_
#define SRC_TBL_H_

#define MAGIC_M 0x37

/*****file format:
magicM(8) | flags(16) | number_group(8)
number_relation(32) | [{rel1_src_GroupId(8) | rel1_target_GroupId(8) | rel1_flag(16) | rel1_src_Idx(32) | rel1_target_Idx(32)}, ...]
group1Id(8)|group1_count(32)|group1_size_byte(32)|[{Flag(16)|SZ(16)|Value(char array)}, ...]
group2Id(8)|group2_count(32)|group2_size_byte(32)|[{Flag(16)|SZ(16)|Value(char array)}, ...]
...
optional checksum at the end.
*/

/*****diagram
code_list(edge, group2) =======>--- word_list(center, group1)
info_list(edge, group3) =======>-----/
pinyin_list(edge, group4) =====>----/
...._list(edge, groupN) =======>---^
##################### the relation direction:
       RelationElement:   =====>-----
ReverseRelationElement:   -----<=====
*/

struct TableRelationElement {
	unsigned char sourceGroupId;
	unsigned char targetGroupId;
	unsigned short flagr;//
	int sourceIdx;
	int targetIdx;
};

//======================================
struct CodeBuffer {
	int capacity;
	int size;
	char *buffer;
};
struct ValueItem {
	//unsigned int idx;
	unsigned short flagv;
	unsigned short valuelen;
	char *value;//point to a position in struct CodeBuffer->buffer.
};
/*
use bsearch to find in the array:
void *bsearch(const void key[.size], const void base[.size * .nmemb],
size_t nmemb, size_t size,
int (*compar)(const void [.size], const void [.size]));
*/
struct ValueTable {
	struct CodeBuffer cbuffer;
	struct ValueItem *vitem;
};
//=======================================

struct CacheTable {
	unsigned *fileOffset;//array, num = @groupCount
	unsigned int numberCache;
	//linked hash cache.
};
//=======================================


struct GroupValueWrapper {
	int type;//full data: 1, partial cached: 2
	union {
		struct ValueTable vt;
		struct CacheTable ct;
	}obj;//ValueTable or CacheTable
};
struct TableGroupInfo {
	unsigned char groupId;
	unsigned int numberValue;//number of item in the group
	unsigned int groupSize;//in byte
	unsigned int startPos;//file offset
	struct GroupValueWrapper groupValue;
};

struct TableInfo {
	unsigned short flag;
	unsigned char numberGroup;
	unsigned char xxxx;
	int numberRelation;
	//int number_reverse_relation;//most likely the same as number_relation
	struct TableRelationElement *relations;//array.
	struct TableRelationElement *reverseRelations;//array
	struct TableGroupInfo *groups;//array
};


#endif /* SRC_TBL_H_ */
