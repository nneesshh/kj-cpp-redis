#include "rdb_parser/rdb_parser.h"

#include <time.h>

struct file_builder {
	const char *path;
	const char *dump_to_path;
	FILE *fp;

	int total;
};

static int
__dump_object(rdb_object_t *o, struct file_builder *fb)
{
	rdb_kv_t *kv;
	rdb_kv_chain_t *kvcl;

	FILE *fp = fb->fp;
	
	++fb->total;

	switch (o->type) {
	case RDB_OPCODE_AUX:
		fprintf(fp, "(%d)[AUX] key(%s)val(%s)\n\n",
			fb->total, o->aux_key.data, o->aux_val.data);
		break;

	case RDB_OPCODE_EXPIRETIME:
	case RDB_OPCODE_EXPIRETIME_MS:
		fprintf(fp, "(%d)[EXPIRE] %d\n\n",
			fb->total, o->expire);
		break;

	case RDB_OPCODE_SELECTDB:
		fprintf(fp, "(%d)[DB_SELECTOR] %d\n\n",
			fb->total, o->db_selector);
		break;

	case RDB_TYPE_STRING:
		fprintf(fp, "(%d)[STRING] %s = %s\n\n",
			fb->total, o->key.data, o->val.data);
		break;

	case RDB_TYPE_LIST:
		fprintf(fp, "(%d)[LIST] %s = (%d)\n[\n",
			fb->total, o->key.data, o->size);

		for (kvcl = o->vall; kvcl; kvcl = kvcl->next) {
			kv = kvcl->kv;
			fprintf(fp, "\t%s,\n", kv->val.data);
		}
		fprintf(fp, "]\n\n");
		break;

	case RDB_TYPE_SET:
		fprintf(fp, "(%d)[SET] %s = (%d)\n[\n",
			fb->total, o->key.data, o->size);

		for (kvcl = o->vall; kvcl; kvcl = kvcl->next) {
			kv = kvcl->kv;
			fprintf(fp, "\t%s,\n", kv->val.data);
		}
		fprintf(fp, "]\n\n");
		break;

	case RDB_TYPE_ZSET:
		fprintf(fp, "(%d)[ZSET] %s = (%d)\n[\n",
			fb->total, o->key.data, o->size);

		for (kvcl = o->vall; kvcl; kvcl = kvcl->next) {
			kv = kvcl->kv;
			fprintf(fp, "\t%s = %s,\n", kv->key.data, kv->val.data);
		}
		fprintf(fp, "]\n\n");
		break;

	case RDB_TYPE_HASH:
		fprintf(fp, "(%d)[HASH] %s = (%d)\n[\n",
			fb->total, o->key.data, o->size);

		for (kvcl = o->vall; kvcl; kvcl = kvcl->next) {
			kv = kvcl->kv;
			fprintf(fp, "\t%s = %s,\n", kv->key.data, kv->val.data);
		}
		fprintf(fp, "]\n\n");
		break;

	case RDB_TYPE_HASH_ZIPMAP:
		fprintf(fp, "(%d)[ZIPMAP] %s = (%d)\n[\n",
			fb->total, o->key.data, o->size);

		for (kvcl = o->vall; kvcl; kvcl = kvcl->next) {
			kv = kvcl->kv;

			if (kv->key.len > 0) {
				fprintf(fp, "\t%s = %s,\n", kv->key.data, kv->val.data);
			}
			else {
				fprintf(fp, "\t%s,\n", kv->val.data);
			}
		}
		fprintf(fp, "]\n\n");
		break;

	case RDB_TYPE_LIST_ZIPLIST:
		fprintf(fp, "(%d)[ZIPLIST] %s = (%d)\n[\n",
			fb->total, o->key.data, o->size);

		for (kvcl = o->vall; kvcl; kvcl = kvcl->next) {
			kv = kvcl->kv;

			if (kv->key.len > 0) {
				fprintf(fp, "\t%s = %s,\n", kv->key.data, kv->val.data);
			}
			else {
				fprintf(fp, "\t%s,\n", kv->val.data);
			}
		}
		fprintf(fp, "]\n\n");
		break;

	case RDB_TYPE_SET_INTSET:
		fprintf(fp, "(%d)[INTSET] %s = (%d)\n[\n",
			fb->total, o->key.data, o->size);

		for (kvcl = o->vall; kvcl; kvcl = kvcl->next) {
			kv = kvcl->kv;

			if (kv->key.len > 0) {
				fprintf(fp, "\t%s = %s,\n", kv->key.data, kv->val.data);
			}
			else {
				fprintf(fp, "\t%s,\n", kv->val.data);
			}
		}
		fprintf(fp, "]\n\n");
		break;

	case RDB_TYPE_ZSET_ZIPLIST:
		fprintf(fp, "(%d)[ZSET_ZL] %s = (%d)\n[\n",
			fb->total, o->key.data, o->size);

		for (kvcl = o->vall; kvcl; kvcl = kvcl->next) {
			kv = kvcl->kv;

			if (kv->key.len > 0) {
				fprintf(fp, "\t%s = %s,\n", kv->key.data, kv->val.data);
			}
			else {
				fprintf(fp, "\t%s,\n", kv->val.data);
			}
		}
		fprintf(fp, "]\n\n");
		break;

	case RDB_TYPE_HASH_ZIPLIST:
		fprintf(fp, "(%d)[HASH_ZL] %s = (%d)\n[\n",
			fb->total, o->key.data, o->size);

		for (kvcl = o->vall; kvcl; kvcl = kvcl->next) {
			kv = kvcl->kv;

			if (kv->key.len > 0) {
				fprintf(fp, "\t%s = %s,\n", kv->key.data, kv->val.data);
			}
			else {
				fprintf(fp, "\t%s,\n", kv->val.data);
			}
		}
		fprintf(fp, "]\n\n");
		break;

	case RDB_OPCODE_EOF:
	default:
		break;
	}

	return 0;
}

static int
on_build_object(rdb_object_t *o, void *payload) {
	struct file_builder *fb = (struct file_builder *)payload;
	__dump_object(o, fb);
	return 0;
}

int main(int argc, char* argv[]) {
	int i, count;
	time_t tmstart, tmover;
	time_t tmstart2, tmover2;
	struct file_builder fb;

	rdb_parser_t *rp;

	count = 10;
	tmstart = time(NULL);

	rp = create_rdb_parser(on_build_object, &fb);

	for (i = 0; i < count; ++i) {

		//fb.path = "data/dump2.8.rdb";
		fb.path = "data/dump3.rdb";
		fb.dump_to_path = "data/dump3.txt";
		fb.fp = fopen(fb.dump_to_path, "w");
		fb.total = 0;

		tmstart2 = time(NULL);

		fprintf(fb.fp, "version:%d\n", rp->version);
		fprintf(fb.fp, "==== dump start ====\n\n");

		rdb_parse_file(rp, fb.path);

		fprintf(fb.fp, "==== dump over ====\n");
		fprintf(fb.fp, "chksum:%lld\n", rp->chksum);
		fprintf(fb.fp, "bytes:%lld\n", rp->parsed);
		fprintf(fb.fp, "objects:%d\n", fb.total);

		tmover2 = time(NULL);
		fprintf(fb.fp, "cost: %lld seconds", tmover2 - tmstart2);
		fclose(fb.fp);

		reset_rdb_parser(rp);
	}

	tmover = time(NULL);
    printf("total = %lld, per_time_cost = %f",
        (tmover - tmstart), (float)(tmover - tmstart) / count);
    destroy_rdb_parser(rp);
    return 0;
}