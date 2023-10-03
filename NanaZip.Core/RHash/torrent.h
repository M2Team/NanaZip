/* torrent.h */
#ifndef TORRENT_H
#define TORRENT_H
#include "sha1.h"

#ifdef __cplusplus
extern "C" {
#endif

#define btih_hash_size  20

/* vector structure */
typedef struct torrent_vect
{
	void** array;     /* array of elements of the vector */
	size_t size;      /* vector size */
	size_t allocated; /* number of allocated elements */
} torrent_vect;

/* a binary string */
typedef struct torrent_str
{
	char* str;
	size_t length;
	size_t allocated;
} torrent_str;

/* BitTorrent algorithm context */
typedef struct torrent_ctx
{
	unsigned char btih[20]; /* resulting BTIH hash sum */
	unsigned options;       /* algorithm options */
	sha1_ctx sha1_context;  /* context for hashing current file piece */
#if defined(USE_OPENSSL) || defined(OPENSSL_RUNTIME)
	unsigned long reserved; /* need more space for OpenSSL SHA1 context */
#endif
	size_t index;             /* byte index in the current piece */
	size_t piece_length;      /* length of a torrent file piece */
	size_t piece_count;       /* the number of pieces processed */
	size_t error;             /* non-zero if error occurred, zero otherwise */
	torrent_vect hash_blocks; /* array of blocks storing SHA1 hashes */
	torrent_vect files;       /* names of files in a torrent batch */
	torrent_vect announce;    /* announce URLs */
	char* program_name;       /* the name of the program */

	torrent_str content;      /* the content of generated torrent file */
#if defined(USE_OPENSSL) || defined(OPENSSL_RUNTIME)
	rhash_hashing_methods sha1_methods;
#endif
} torrent_ctx;

void bt_init(torrent_ctx* ctx);
void bt_update(torrent_ctx* ctx, const void* msg, size_t size);
void bt_final(torrent_ctx* ctx, unsigned char result[20]);
void bt_cleanup(torrent_ctx* ctx);

#if !defined(NO_IMPORT_EXPORT)
size_t bt_export(const torrent_ctx* ctx, void* out, size_t size);
size_t bt_import(torrent_ctx* ctx, const void* in, size_t size);
#endif /* !defined(NO_IMPORT_EXPORT) */

unsigned char* bt_get_btih(torrent_ctx* ctx);
size_t bt_get_text(torrent_ctx* ctx, char** pstr);

/* possible options */
#define BT_OPT_PRIVATE 1
#define BT_OPT_INFOHASH_ONLY 2
#define BT_OPT_TRANSMISSION 4

void bt_set_options(torrent_ctx* ctx, unsigned options);
int  bt_add_file(torrent_ctx* ctx, const char* path, uint64_t filesize);
int  bt_add_announce(torrent_ctx* ctx, const char* announce_url);
int  bt_set_program_name(torrent_ctx* ctx, const char* name);
void bt_set_piece_length(torrent_ctx* ctx, size_t piece_length);
void bt_set_total_batch_size(torrent_ctx* ctx, uint64_t total_size);
size_t bt_default_piece_length(uint64_t total_size, int transmission);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* TORRENT_H */
