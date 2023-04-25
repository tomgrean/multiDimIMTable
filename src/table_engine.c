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

#include "tbl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct GroupValueIterator {
	const struct TableInfo *ptbl;
	char query[256];
	unsigned char querylen;//length of query.
	const unsigned char groupId;
	const unsigned char flag;
	char match;//1: found, 0: not found.
	int nextIdx;
};
struct RelationIterator {
	const struct TableInfo *ptbl;
	int nextIdx;//relation index
};
struct ReverseRelationIterator {
	const struct TableInfo *ptbl;
	int nextIdx;//relation index
};



int load_header_data(FILE *ifile, struct TableInfo *tbl)
{
	unsigned char dbyte;
	unsigned short dshort;
	size_t ret;
	if (!(ifile && tbl)) {
		printf("error null parameter\n");
		return -1;
	}

	ret = fread(&dbyte, 1, 1, ifile);
	if (1 != ret || MAGIC_M != dbyte) {
		printf("error corrupt file\n");
		return 1;
	}
	tbl->xxxx = dbyte;
	ret = fread(&dshort, 2, 1, ifile);
	if (1 != ret) {
		printf("error read flag\n");
		return 1;
	}
	tbl->flag = dshort;

	ret = fread(&dbyte, 1, 1, ifile);
	if (1 != ret) {
		printf("error read numberGroup\n");
		return 1;
	}
	tbl->numberGroup = dbyte;
	return 0;
}
int load_relation_data(FILE *ifile, struct TableInfo *tbl)
{
	unsigned int i, dnum;
	struct TableRelationElement *tre;
	size_t ret;

	ret = fread(&dnum, 4, 1, ifile);
	if (1 != ret) {
		printf("error read number of relation\n");
		return 1;
	}
	tre = malloc(dnum * sizeof(struct TableRelationElement));
	if (!tre) {
		printf("error malloc TableRelationElement\n");
		return 2;
	}
	for (i = 0; i < dnum; ++i) {
		ret = fread(&(tre[i].sourceGroupId), 1, 1, ifile);
		if (1 != ret) {
			printf("error read relation src grp id %u\n", i);
			return 1;
		}
		ret = fread(&(tre[i].targetGroupId), 1, 1, ifile);
		if (1 != ret) {
			printf("error read relation tar grp id %u\n", i);
			return 1;
		}
		ret = fread(&(tre[i].flagr), 2, 1, ifile);
		if (1 != ret) {
			printf("error read relation flagr id %u\n", i);
			return 1;
		}
		ret = fread(&(tre[i].sourceIdx), 4, 1, ifile);
		if (1 != ret) {
			printf("error read relation src idx id %u\n", i);
			return 1;
		}
		ret = fread(&(tre[i].targetIdx), 4, 1, ifile);
		if (1 != ret) {
			printf("error read relation tar idx id %u\n", i);
			return 1;
		}
	}
	tbl->relations = tre;
	tbl->numberRelation = dnum;
	return 0;
}

static int reverse_relation_cmp(const void *r1, const void *r2)
{
	const struct TableRelationElement *re1 = r1, *re2 = r2;
	int result;
	if (re1 == re2) {
		return 0;
	}
	result = re1->targetGroupId - re2->targetGroupId;
	if (!result) {
		result = re1->targetIdx - re2->targetIdx;
		if (!result) {
			result = re1->sourceGroupId - re2->sourceGroupId;
			if (!result) {
				return re1->sourceIdx - re2->sourceIdx;//TODO: not a good idea.
			}
		}
	}
	return result;
}

int load_reverse_relation_data(struct TableInfo *tbl)
{
	tbl->reverseRelations = malloc(tbl->numberRelation * sizeof(struct TableRelationElement));
	if (!tbl->reverseRelations) {
		printf("error malloc reverse relation\n");
		return 2;
	}
	memcpy(tbl->reverseRelations, tbl->relations, tbl->numberRelation * sizeof(struct TableRelationElement));
	qsort(tbl->reverseRelations, tbl->numberRelation, sizeof(struct TableRelationElement), reverse_relation_cmp);
	return 0;
}
int load_group_data(FILE *ifile, struct TableInfo *tbl)
{
	unsigned int i;
	size_t ret;
	tbl->groups = malloc(tbl->numberGroup * sizeof(struct TableGroupInfo));
	if (! tbl->groups) {
		printf("error malloc groups:%u\n", tbl->numberGroup);
		return 2;
	}
	for (i = 0; i < tbl->numberGroup; ++i) {
		ret = fread(&(tbl->groups[i].groupId), 1, 1, ifile);
		if (1 != ret) {
			printf("error read groupId\n");
			return 1;
		}
		ret = fread(&(tbl->groups[i].numberValue), 4, 1, ifile);
		if (1 != ret) {
			printf("error read group count");
			return 1;
		}
		ret = fread(&(tbl->groups[i].groupSize), 4, 1, ifile);
		if (1 != ret) {
			printf("error read group size\n");
			return 1;
		}
		tbl->groups[i].startPos = ftell(ifile);
		fseek(ifile, tbl->groups[i].groupSize, SEEK_CUR);
	}
	return 0;
}
int load_full_code_buffer(FILE *ifile, struct TableInfo *tbl)
{
	unsigned int i;

	for (i = 0; i < tbl->numberGroup; ++i) {
		struct TableGroupInfo *tgi = &(tbl->groups[i]);
		struct ValueTable *pvt = &(tgi->groupValue.obj.vt);
		unsigned int z;
		unsigned int ret;
		ret = fseek(ifile, tgi->startPos, SEEK_SET);
		if (ret) {
			printf("seek file for code error\n");
			return ret;
		}
		tgi->groupValue.type = 1;
		pvt->vitem = malloc(tgi->numberValue * sizeof(struct ValueItem));
		if (!pvt->vitem) {
			printf("malloc for code value item failed\n");
			return 1;
		}
		pvt->cbuffer.buffer = malloc(tgi->groupSize - tgi->numberValue * (2 + 2));
		if (!pvt->cbuffer.buffer) {
			printf("malloc for code value buffer failed\n");
			return 1;
		}
		pvt->cbuffer.capacity = tgi->groupSize - tgi->numberValue * (2 + 2);
		pvt->cbuffer.size = 0;

		for (z = 0; z < tgi->numberValue; ++z) {
			ret = fread(&(pvt->vitem[z].flagv), 2, 1, ifile);
			if (1 != ret) {
				printf("read code value flagv failed\n");
				return 1;
			}
			ret = fread(&(pvt->vitem[z].valuelen), 2, 1, ifile);
			if (1 != ret) {
				printf("read code valuelen failed\n");
				return 1;
			}
			if (0 == pvt->vitem[z].valuelen) {
				printf("================invalie length:%u\n", pvt->vitem[z].valuelen);
			}

			pvt->vitem[z].value = pvt->cbuffer.buffer + pvt->cbuffer.size;
			ret = fread(pvt->vitem[z].value, 1, pvt->vitem[z].valuelen, ifile);
			if (pvt->vitem[z].valuelen != ret) {
				printf("read code value buffer failed\n");
				return 1;
			}
			pvt->cbuffer.size += ret;
		}
	}
	return 0;
}

static int word_search_cmp(const void *e1, const void *e2)
{
	const struct ValueItem *ve1 = e1, *ve2 = e2;
	//similar to strncmp.
	const char *p1 = ve1->value, *p2 = ve2->value;
	int i = 0, ret;
	//assert ve1->valuelen > 0 && ve2->valuelen > 0
	while ((ret = *p1 - *p2) == 0) {
		++p1;
		++p2;
		++i;
		if (i >= ve1->valuelen) {
			if (i >= ve2->valuelen) {
				return 0;
			}
			return -1;
		}
		if (i >= ve2->valuelen) {
			return 1;
		}
	}
	return ret;
}
static int relation_search_cmp(const void *e1, const void *e2)
{
	const struct TableRelationElement *re1 = e1, *re2 = e2;
	int result;
	if (re1 == re2) {
		return 0;
	}
	result = re1->sourceGroupId - re2->sourceGroupId;
	if (!result) {
		result = re1->sourceIdx - re2->sourceIdx;
		if (!result) {
			return re1->targetGroupId - re2->targetGroupId;
		}
	}
	return result;
}

//NOTE: it is not the same as @reverse_relation_cmp()
static int reverse_search_cmp(const void *r1, const void *r2)
{
	const struct TableRelationElement *re1 = r1, *re2 = r2;
	int result;
	if (re1 == re2) {
		return 0;
	}
	result = re1->targetGroupId - re2->targetGroupId;
	if (!result) {
		result = re1->targetIdx - re2->targetIdx;
		if (!result) {
			return re1->sourceGroupId - re2->sourceGroupId;
		}
	}
	return result;
}

//on  fount, return the found first pointer, and *len as its index.
//not found, return NULL, and *len as hinted index that goes after the searched key.
void *hintBsearch(const void *key, const void *arr, int *len, int size, int (*cmp)(const void*, const void*))
{
	int lim, ret = -1;
	const void *p;
	const void *base = arr;

	for (lim = *len; lim != 0; lim >>= 1) {
		p = base + (lim >> 1) * size;
		ret = cmp(key, p);
		if (ret == 0) {
			for (base = p - size; base >= arr && cmp(key, base) == 0; base -= size) {
				//nothing.
				p = base;
			}
			*len = (p - arr) / size;
			return (void*)p;
		}
		if (ret > 0) {	/* key > p: move right */
			base = p + size;
			--lim;
		} /* else move left */
	}
	*len = (p - arr) / size;
	if (ret > 0) {
		++*len;
	}
	return (NULL);
}


//return the value length. on error, return <0
int getGroupValue(const struct GroupValueIterator *gvit, char buffer[256])
{
	if (!gvit || gvit->nextIdx < 0) {
		return -1;
	}
	const struct GroupValueWrapper *pgv = &(gvit->ptbl->groups[gvit->groupId].groupValue);
	switch (pgv->type) {
	case 1://full data
	{
		const struct ValueItem *vitm = &(pgv->obj.vt.vitem[gvit->nextIdx]);
		memcpy(buffer, vitm->value, vitm->valuelen);
		buffer[vitm->valuelen] = 0;
		return (int)vitm->valuelen;
	}
	case 2://partial cache
	default:
		printf("not implemented\n");
		break;
	}
	return -1;
}
//return true on OK, false on failure.
int nextGroupValue(struct GroupValueIterator *gvit)
{
	++(gvit->nextIdx);
	struct TableGroupInfo *ptgi = &(gvit->ptbl->groups[gvit->groupId]);

	if (gvit->nextIdx >= ptgi->numberValue) {
		gvit->nextIdx = -1;
		return 0;
	}
	switch (ptgi->groupValue.type) {
	case 1://full data
	{
		struct ValueItem *pvi = &(ptgi->groupValue.obj.vt.vitem[gvit->nextIdx]);
		if (memcmp(gvit->query, pvi->value, gvit->querylen)) {
			gvit->nextIdx = -1;
			return 0;
		}
		//TODO match query by wild_card string like *.
	}
		break;
	case 2://partial cache..
	default:
		gvit->nextIdx = -1;
		return 0;
	}
	return 1;
}
//return the value length. on error, return <0
int getTargetValue(const struct RelationIterator *rit, char buffer[256])
{
	if (!rit || rit->nextIdx < 0) {
		return -1;
	}
	const unsigned char targetGroupId = rit->ptbl->relations[rit->nextIdx].targetGroupId;
	const int targetIdx = rit->ptbl->relations[rit->nextIdx].targetIdx;
	const struct GroupValueWrapper *pgv = &(rit->ptbl->groups[targetGroupId].groupValue);
	switch (pgv->type) {
	case 1://full data
	{
		const struct ValueItem *vitm = &(pgv->obj.vt.vitem[targetIdx]);
		memcpy(buffer, vitm->value, vitm->valuelen);
		buffer[vitm->valuelen] = 0;
		return (int)vitm->valuelen;
	}
	case 2://partial cache
	default:
		printf("not implemented\n");
		break;
	}
	return -1;
}
//return true on OK, false on failure.
int nextRelation(struct RelationIterator *rit)
{
	if (rit->nextIdx < 0) {
		return 0;
	}
	const struct TableRelationElement *ptre = rit->ptbl->relations;
	const unsigned char sourceGroupId = ptre[rit->nextIdx].sourceGroupId;
	const int sourceIdx = ptre[rit->nextIdx].sourceIdx;
	++(rit->nextIdx);
	if (rit->nextIdx >= rit->ptbl->numberRelation
			|| ptre[rit->nextIdx].sourceIdx != sourceIdx
			|| ptre[rit->nextIdx].sourceGroupId != sourceGroupId) {
		rit->nextIdx = -1;
		return 0;
	}
	return 1;
}
//return the value length. on error, return <0
int getSourceValue(const struct ReverseRelationIterator *rit, char buffer[256])
{
	if (!rit || rit->nextIdx < 0) {
		return -1;
	}
	const unsigned char sourceGroupId = rit->ptbl->reverseRelations[rit->nextIdx].sourceGroupId;
	const int sourceIdx = rit->ptbl->reverseRelations[rit->nextIdx].sourceIdx;
	const struct GroupValueWrapper *pgv = &(rit->ptbl->groups[sourceGroupId].groupValue);
	switch (pgv->type) {
	case 1://full data
	{
		const struct ValueItem *vitm = &(pgv->obj.vt.vitem[sourceIdx]);
		memcpy(buffer, vitm->value, vitm->valuelen);
		buffer[vitm->valuelen] = 0;
		return (int)vitm->valuelen;
	}
	case 2://partial cache
	default:
		printf("not implemented\n");
		break;
	}
	return -1;
}
//return true on OK, false on failure.
int nextReverseRelation(struct ReverseRelationIterator *rit)
{
	if (rit->nextIdx < 0) {
		return 0;
	}
	const struct TableRelationElement *ptre = rit->ptbl->reverseRelations;
	const unsigned char targetGroupId = ptre[rit->nextIdx].targetGroupId;
	const int targetIdx = ptre[rit->nextIdx].targetIdx;
	++(rit->nextIdx);
	if (rit->nextIdx >= rit->ptbl->numberRelation
			|| ptre[rit->nextIdx].targetIdx != targetIdx
			|| ptre[rit->nextIdx].targetGroupId != targetGroupId) {
		rit->nextIdx = -1;
		return 0;
	}
	return 1;
}

//search for @q in @groupId, return the iterator.
//@matchFlag: 0x10: use wildcard search. wildcard char is '\0'
struct GroupValueIterator searchGroupValue(const struct TableInfo *ptbl, unsigned char groupId, unsigned char matchFlag, unsigned char qlen, const char *q)
{
	int n;
	const struct ValueItem tkey = {.flagv = 0, .valuelen = strlen(q), .value = (char*)q};
	struct GroupValueIterator result = {.ptbl = ptbl, .querylen = qlen, .groupId = groupId, .flag = matchFlag, .match = 0, .nextIdx = -1};
	struct ValueTable *pvt;
	struct ValueItem *retvi = NULL;

	if (!(ptbl && q && *q && qlen < 256)) {
		return result;
	}
	if (qlen <= 0) {
		qlen = strlen(q);
		result.querylen = qlen;
	}
	memcpy(result.query, q, qlen);
	switch (ptbl->groups[groupId].groupValue.type) {
	case 1://full data.
		pvt = &(ptbl->groups[groupId].groupValue.obj.vt);
		n = ptbl->groups[groupId].numberValue;
		retvi = hintBsearch(&tkey, pvt->vitem, &n, sizeof(struct ValueItem), word_search_cmp);
		if (retvi) {
			result.match = 1;
			result.nextIdx = n;
		} else {
			if (0 == memcmp(result.query, pvt->vitem[n].value, result.querylen)) {
				result.nextIdx = n;
			//} else if (result.query & 0x10) {//wild
				//........
			}
		}
		break;
	case 2://partial cache.
	default:
		printf("not implemented!\n");
		break;
	}
	//printf("OK %d %d in search Group Value!\n", result.match, result.nextIdx);
	return result;
}
//get relationship iterator from @gvit
struct RelationIterator searchRelation(const struct GroupValueIterator *gvit)
{
	struct RelationIterator result = {.ptbl = NULL, .nextIdx = -1};
	if (!gvit || gvit->nextIdx < 0) {
		return result;
	}
	int n;
	const struct TableRelationElement tkey = {
			.sourceGroupId = gvit->groupId,
			.targetGroupId = 0,//the main group.
			.flagr = 0,
			.sourceIdx = gvit->nextIdx,
			.targetIdx = 0
	};
	struct TableRelationElement *tret;
	result.ptbl = gvit->ptbl;

	//printf("getting the result! idx=%d\n", gvit->nextIdx);

	n = gvit->ptbl->numberRelation;
	tret = hintBsearch(&tkey, gvit->ptbl->relations, &n, sizeof(struct TableRelationElement), relation_search_cmp);
	if (tret) {
		result.nextIdx = n;
	}
	return result;
}
//each RelationIterator can have a corresponding sourceGroupId:ReverseRelationIterator pair.
struct ReverseRelationIterator searchReverseRelation(const struct RelationIterator *rit, unsigned char sourceGroupId)
{
	struct ReverseRelationIterator result = {.ptbl = NULL, .nextIdx = -1};
	if (!rit || rit->nextIdx < 0) {
		return result;
	}
	int n = rit->ptbl->relations[rit->nextIdx].targetIdx;
	const struct TableRelationElement tkey = {
			.sourceGroupId = sourceGroupId,
			.targetGroupId = 0,//the main groupId is 0
			.flagr = 0,
			.sourceIdx = 0,
			.targetIdx = n
	};
	struct TableRelationElement *tret;
	result.ptbl = rit->ptbl;

	//printf("getting the reverse result! idx=%d\n", n);

	n = rit->ptbl->numberRelation;
	tret = hintBsearch(&tkey, rit->ptbl->reverseRelations, &n, sizeof(struct TableRelationElement), reverse_search_cmp);
	if (tret) {
		result.nextIdx = n;
	}
	return result;
}

int load_from_file(struct TableInfo *ptbl, FILE *ifile)
{
	int ret;

	memset(ptbl, 0, sizeof(struct TableInfo));
	do {
		ret = load_header_data(ifile, ptbl);
		if (ret) {
			break;
		}
		ret = load_relation_data(ifile, ptbl);
		if (ret) {
			break;
		}
		ret = load_group_data(ifile, ptbl);
		if (ret) {
			break;
		}
		ret = load_reverse_relation_data(ptbl);
		if (ret) {
			break;
		}

		//optional in lazy mode.
		ret = load_full_code_buffer(ifile, ptbl);
		if (ret) {
			break;
		}

		return 0;
	} while (0);

	//clean up the data.
	if (ptbl->groups) {
		unsigned int z;
		for (z = 0; z < ptbl->numberGroup; ++z) {
			if (1 == ptbl->groups[z].groupValue.type) {
				free(ptbl->groups[z].groupValue.obj.vt.cbuffer.buffer);
				free(ptbl->groups[z].groupValue.obj.vt.vitem);
			} else if (2 == ptbl->groups[z].groupValue.type) {
				printf("clear cache not implemented!\n");
			}
		}
		free(ptbl->groups);
	}
	if (ptbl->relations) {
		free(ptbl->relations);
	}
	if (ptbl->reverseRelations) {
		free(ptbl->reverseRelations);
	}
	return -1;
}
int main(int argc, char *argv[])
{
	int ret;
	struct TableInfo tbl;

	if (argc != 2) {
		printf("usage: %s table.mb\n", argv[0]);
		return 1;
	}
	FILE *ifile = fopen(argv[1], "rb");

	if (!ifile) {
		printf("error open table!\n");
		return 1;
	}
	ret = load_from_file(&tbl, ifile);
	fclose(ifile);
	printf("===========load file end===========%d\n", ret);
	//my work goes...
	//test
	while (1) {
		char buffer[256];
		for (ret = 0; ret < 256; ++ret) {
			int c = getchar();
			if (EOF == c) {
				ret = -1;
				break;
			}
			if ('\n' == c) {
				break;
			}
			buffer[ret] = c;
		}
		if (ret <= 0) {
			printf("====EOF====\n");
			break;
		}
		buffer[ret] = 0;
		struct GroupValueIterator gvit = searchGroupValue(&tbl, 1, 0, ret, buffer);
		if (gvit.match) {
			printf("exact match\n");
		} else {
			ret = getGroupValue(&gvit, buffer);
			printf("no match, using hinted:%d %s\n", ret, buffer);
		}
		for (struct RelationIterator rit = searchRelation(&gvit);
				rit.nextIdx >= 0;
				nextRelation(&rit)) {
			ret = getTargetValue(&rit, buffer);
			printf(">>%d %s\n", ret, buffer);
			for (struct ReverseRelationIterator rrit = searchReverseRelation(&rit, 2);
					rrit.nextIdx >= 0;
					nextReverseRelation(&rrit)) {
				ret = getSourceValue(&rrit, buffer);
				printf("Revereslookup>>%d %s\n", ret, buffer);
			}
		}
		printf("---next hinted---\n");
		if (nextGroupValue(&gvit)) {
			ret = getGroupValue(&gvit, buffer);
			printf("hinted code^^%d %s\n", ret, buffer);
			for (struct RelationIterator rit = searchRelation(&gvit);
					rit.nextIdx >= 0;
					nextRelation(&rit)) {
				ret = getTargetValue(&rit, buffer);
				printf("^^%d %s\n", ret, buffer);
				for (struct ReverseRelationIterator rrit = searchReverseRelation(&rit, 2);
						rrit.nextIdx >= 0;
						nextReverseRelation(&rrit)) {
					ret = getSourceValue(&rrit, buffer);
					printf("Revereslookup^^%d %s\n", ret, buffer);
				}
			}
		}
		printf("==========match==end===========%d\n", ret);
	}

	//TODO clear the memory.
	return 0;
}


