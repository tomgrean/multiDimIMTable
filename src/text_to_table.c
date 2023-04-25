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

#ifdef __linux
#include <bsd/sys/tree.h>
#else//bsd
#include <sys/tree.h>
#endif
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include "tbl.h"
#define MAGIC_M 0x37

struct node {
	RB_ENTRY(node) entry;
	int idx;
	int len;
	int flag;
	char *buf;
};


struct RelationElement {
	unsigned char sourceGroupId;
	unsigned char targetGroupId;
	unsigned short flagr;
	int lineno;//when both sourceGroupId and *psrcIdx are equal, compare lineno.
	int *psrcIdx;
	int *ptargetIdx;
};

static int relation_cmp(const void *r1, const void *r2)
{
	const struct RelationElement *re1 = r1, *re2 = r2;
	int result;
	if (re1 == re2) {
		return 0;
	}
	result = re1->sourceGroupId - re2->sourceGroupId;
	if (!result) {
		result = *(re1->psrcIdx) - *(re2->psrcIdx);
		if (!result) {
			return re1->lineno - re2->lineno;
		}
	}
	return result;
}

int tablecmp(struct node *e1, struct node *e2) {
	int i;
	for (i = 0; i < e1->len; ++i) {
		int ret;
		if (i >= e2->len) {
			return 1;//e1 is larger.
		}
		ret = e1->buf[i] - e2->buf[i];
		if (ret) {
			return ret;
		}
	}
	return i == e2->len ? 0 : -1;
}

RB_HEAD(tabletree, node);
RB_PROTOTYPE(tabletree, node, entry, tablecmp)
RB_GENERATE(tabletree, node, entry, tablecmp)

//array list
#define ARRAYLIST_DEFINE(arr, type) struct arr {\
	unsigned int array_cap;\
	unsigned int array_len;\
	type *val;\
}
#define ARRAYLIST_GENERATE(arr, type) \
	int arr##_init(struct arr *a, int size) {\
		a->val = malloc(sizeof(type) * size);\
		if (a->val) {a->array_cap=size; a->array_len=0; return 0;}\
		return -1;}\
	void arr##_destroy(struct arr *a) {\
		free(a->val);}\
	type *arr##_append(struct arr *a) {\
		type *result;\
		if (a->array_len >= a->array_cap) {\
		void *tmp = realloc(a->val, (2 * sizeof(type)) * a->array_cap);\
		if (!tmp) {return NULL;}\
		a->val = tmp; a->array_cap *= 2;\
		}\
		result=&(a->val[a->array_len]); ++(a->array_len);\
		return result;}\
	int arr##_swap(struct arr *a, int idx1, int idx2) {\
		if (idx1 < 0 || idx2 < 0 || idx1 >= a->array_len || idx2 >= a->array_len) {return -1;}\
		{type tmp;\
		memcpy(&tmp, &(a->val[idx1]), sizeof(type));\
		memcpy(&(a->val[idx1]), &(a->val[idx2]), sizeof(type));\
		memcpy(&(a->val[idx2]), &tmp, sizeof(type));}\
		return 0;}\
	type *arr##_get(struct arr *a, int idx) {\
		return &(a->val[idx]);}

#define ARRAYLIST_INIT(arr, a, size) arr##_init(a, size)
#define ARRAYLIST_APPEND(arr, a) arr##_append(a)
#define ARRAYLIST_DESTROY(arr, a) arr##_destroy(a)
#define ARRAYLIST_GET(arr, a, idx) arr##_get(a, idx)

ARRAYLIST_DEFINE(rela, struct RelationElement) grelation;
ARRAYLIST_GENERATE(rela, struct RelationElement)

void print_tree(struct node *n) {
	struct node *left, *right;

	if (n == NULL) {
		printf("nil");
		return;
	}
	left = RB_LEFT(n, entry);
	right = RB_RIGHT(n, entry);
	if (left == NULL && right == NULL)
		printf("%d %d:%s", n->idx, n->len, n->buf);
	else {
		printf("%d %d :%s(", n->idx, n->len, n->buf);
		print_tree(left);
		printf(",");
	}
}

//fidx start from 1.
static int generateTree(char *fbuffer, struct tabletree *headtable, int fidx)
{
	char *buff = fbuffer;
	char *pline;
	int lineno = 0;

	while ((pline = strsep(&buff, "\n\r"))) {
		char *pword;
		int colId = -1;
		int *mainIdx = NULL;

		if (!*pline) {
			continue;//skip empty lines
		}
		if ('#' == *pline) {
			continue;//skip comment
		}
		++lineno;

		while ((pword = strsep(&pline, "\t"))) {
			struct node ntest;
			struct node *n = NULL, *oldn;
			struct RelationElement *rel;
			++colId;
			if (!*pword) {
				continue;
			}

			if (colId > 1) {
				err(2, "unexpected col:%d %s\n", colId, pword);
				break;
			}
			memset(&ntest, 0, sizeof(struct node));
			ntest.buf = pword;
			ntest.len = pline ? pline - pword - 1 : strlen(pword);
			oldn = RB_FIND(tabletree, headtable + colId * fidx, &ntest);
			if (!oldn) {
				n = malloc(sizeof(struct node));
				if (!n) {
					err(2, "malloc node failed!\n");
					return 2;
				}
				memset(n, 0, sizeof(struct node));
				n->buf = pword;
				n->len = pline ? pline - pword - 1 : strlen(pword);
				RB_INSERT(tabletree, headtable + colId * fidx, n);
				oldn = n;
			}
			if (0 == colId) {
				mainIdx = &(oldn->idx);
			} else if (1 == colId) {
				rel = ARRAYLIST_APPEND(rela, &grelation);
				rel->sourceGroupId = fidx;
				rel->targetGroupId = 0;
				rel->flagr = 0xffff;
				rel->lineno = lineno;
				rel->psrcIdx = &(oldn->idx);
				rel->ptargetIdx = mainIdx;
			}
		}
	}
	return 0;
}
static int walktabletree(struct tabletree *tr, int *count, int *bytesize)
{
	int cnt = 0;
	int bsize = 0;
	struct node *n;

	RB_FOREACH(n, tabletree, tr) {
		n->idx = cnt++;
		//printf("%d %lu %s\n", n->len, strlen(n->buf), n->buf);
		//[{Flag(16)|SZ(16)|Value(char array)}, ...]
		bsize += (2 + 2 + n->len);
	}
	if (count)
		*count = cnt;
	if (bytesize)
		*bytesize = bsize;
	return cnt;
}

int table_write_header(FILE *of, int groupNum);
int table_write_relation(FILE *of, struct rela *rel);
int table_write_group(FILE *of, int groupId, int groupNum, int groupSize, struct tabletree *table);

// table writers.
int table_write_header(FILE *of, int groupNum)
{
	unsigned char cc = MAGIC_M;

	//magicM(8) | flags(16) | number_group(8)
	if (groupNum > 30) {
		err(1, "group num %d too large\n", groupNum);
		return -1;
	}

	fwrite(&cc, 1, 1, of);
	//TODO global flags
	cc = 0;
	fwrite(&cc, 1, 2, of);
	cc = groupNum;
	fwrite(&cc, 1, 1, of);
	return 0;
}
int table_write_relation(FILE *of, struct rela *rel)
{
	int i;

	//number_relation(32) | [{rel1_src_GroupId(8) | rel1_target_GroupId(8) | rel1_flag(16) | rel1_src_Idx(32) | rel1_target_Idx(32)}, ...]
	fwrite(&(rel->array_len), 4, 1, of);
	for (i = 0; i < rel->array_len; ++i) {
		struct RelationElement *pre;
		pre = &(rel->val[i]);
		fwrite(&(pre->sourceGroupId), 1, 1, of);
		fwrite(&(pre->targetGroupId), 1, 1, of);
		fwrite(&(pre->flagr), 2, 1, of);
		fwrite(pre->psrcIdx, 4, 1, of);
		fwrite(pre->ptargetIdx, 4, 1, of);
	}
	return 0;
}

int table_write_group(FILE *of, int groupId, int groupNum, int groupSize, struct tabletree *table)
{
	struct node *n;
	//group1Id(8)|group1_count(32)|group1_size_byte(32)|[{Flag(16)|SZ(16)|Value(char array)}, ...]
	unsigned char gid = groupId;
	unsigned short strsize;
	fwrite(&gid, 1, 1, of);
	fwrite(&groupNum, 4, 1, of);
	fwrite(&groupSize, 4, 1, of);
	RB_FOREACH(n, tabletree, table) {
		strsize = n->flag;
		fwrite(&(strsize), 2, 1, of);
		strsize = n->len;
		fwrite(&(strsize), 2, 1, of);
		fwrite(n->buf, 1, n->len, of);
	}
	return 0;
}
//T_ttttttttttttttttttttttttttttttttttttttttttttt
//command arg: ./a.out g0g1.txt g0g2.txt ... outTable.mb
int main(int argc, char *argv[]) {
	int i, v;
	char *fbuffer;
	FILE *wordcodeinfofile;
	struct tabletree *headtable;// = RB_INITIALIZER(&headword);
	int *hlen;
	int *hbytes;

	if (argc <= 2) {
		printf("Invalid argument.\n"
				"Usage: %s g0g1.txt g0g2.txt... outTable.mb\n"
				"Example: %s word-code.txt word-pinyin.txt outTable.mb\n", argv[0], argv[0]);
		return 1;
	}
	--argc;//first omit the last arg.
	headtable = malloc(argc * sizeof(struct tabletree));
	memset(headtable, 0, argc * sizeof(struct tabletree));

	hlen = malloc(argc * sizeof(int));
	hbytes = malloc(argc * sizeof(int));
	memset(hlen, 0, argc * sizeof(int));
	memset(hbytes, 0, argc * sizeof(int));

	printf("arg count: %d\n", argc - 1);
	ARRAYLIST_INIT(rela, &grelation, 16);
	for (i = 1; i < argc; ++i) {
		int filesize;
		wordcodeinfofile = fopen(argv[i], "r");
		if (!wordcodeinfofile) {
			err(1, "fopen %s failed\n", argv[i]);
			return 1;
		}
		fseek(wordcodeinfofile, 0, SEEK_END);
		filesize = ftell(wordcodeinfofile);
		fbuffer = malloc(filesize + 1);
		if (!fbuffer) {
			fclose(wordcodeinfofile);
			err(1, "malloc %d failed\n", i + 1);
			return 1;
		}
		fseek(wordcodeinfofile, 0, SEEK_SET);
		v = fread(fbuffer, 1, filesize, wordcodeinfofile);
		fclose(wordcodeinfofile);
		fbuffer[filesize] = 0;
		if (v != filesize) {
			free(fbuffer);
			err(1, "fread failed: %d != %d\n", v, filesize);
			return 1;
		}

		v = generateTree(fbuffer, headtable, i);
		if (v) {
			err(1, "generate tree failed: %d\n", v);
			return v;
		}

	}

	for (i = 0; i < argc; ++i) {
		printf("****** %d\n", i);
		walktabletree(headtable + i, hlen + i, hbytes + i);
	}
	//sort after the walk.
	qsort(grelation.val, grelation.array_len, sizeof(struct RelationElement), relation_cmp);
	printf("============================%d, %d===============\n", grelation.array_len, grelation.array_cap);

//	//=========debug
//	for (i = 0; i < 100; ++i) {
//		printf("relation[%d]:srcG= %u, tarG= %u, srcIdx=%d, tarIdx=%d\n"
//				,i
//				,grelation.val[i].sourceGroupId
//				,grelation.val[i].targetGroupId
//				,*(grelation.val[i].psrcIdx)
//				,*(grelation.val[i].ptargetIdx)
//				);
//	}

	//write file. argc is decreased by 1.
	wordcodeinfofile = fopen(argv[argc], "wb");
	if (!wordcodeinfofile) {
		err(1, "error open file to write\n");
		return 1;
	}
	table_write_header(wordcodeinfofile, 3);
	printf("==header size %ld\n", ftell(wordcodeinfofile));
	table_write_relation(wordcodeinfofile, &grelation);
	printf("==relation size %ld\n", ftell(wordcodeinfofile));
	//table_write_reverse_relation(wordcodeinfofile, &lrev_rela);
	printf("==reverse_relation size %ld\n", ftell(wordcodeinfofile));
	//foreach group.
	for (i = 0; i < argc; ++i) {
		table_write_group(wordcodeinfofile, i, hlen[i], hbytes[i], headtable + i);
		printf("==table_word size %ld\n", ftell(wordcodeinfofile));
	}
	//endforeach
	//clean up the relation structures...
	fclose(wordcodeinfofile);
	ARRAYLIST_DESTROY(rela, &grelation);
	free(hlen);
	free(hbytes);
	//TODO clean up the tree structures...
	free(headtable);
	return 0;
}

