/* Etc/zsh-development-guide */

#define BUILTIN(name, flags, handler, min, max, funcid, optstr, defopts) \
	{ \
		{NULL, name, flags}, handler, min, max, funcid, optstr, \
			defopts \
	}

enum {
	MN_INTEGER = 1,
	MN_FLOAT = 2,
};

enum {
	META_USEHEAP = 1,
};

enum {
	QT_SINGLE_OPTIONAL = 6,
};

typedef struct builtin *Builtin;
typedef struct features *Features;
typedef struct hashnode *HashNode;
typedef struct linknode *LinkNode;
typedef struct linkedmod *Linkedmod;
typedef union linkroot *LinkList;
typedef void *Module;
typedef void *Options;
typedef void *Param;
typedef void *Shfunc;
typedef struct hashtable *HashTable;

typedef int (*HandlerFunc)(char *, char **, Options, int);
typedef void (*FreeFunc)(void *);
typedef HashNode (*GetNodeFunc)(HashTable, const char *);

typedef struct {
	union {
		long l;
		double d;
	} u;
	int type;
} mnumber;

struct hashnode {
	HashNode next;
	char *nam;
	int flags;
};

struct hashtable {
	int hsize;
	int ct;
	void *nodes;
	void *tmpdata;

	void *hash;
	void *emptytable;
	void *filltable;
	void *cmpnodes;
	void *addnode;
	GetNodeFunc getnode;
	void *getnode2;
	void *removenode;
	void *disablenode;
	void *enablenode;
	void *freenode;
	void *printnode;
	void *scantab;
};
struct builtin {
	struct hashnode node;
	HandlerFunc handlerfunc;
	int minargs;
	int maxargs;
	int funcid;
	char *optstr;
	char *defopts;
};

struct features {
	Builtin bn_list;
	int bn_size;
	void *cd_list;
	int cd_size;
	void *mf_list;
	int mf_size;
	void *pd_list;
	int pd_size;
	int n_abstract;
};

struct linknode {
	LinkNode next;
	LinkNode prev;
	void *dat;
};

struct linklist {
	LinkNode first;
	LinkNode last;
	int flags;
};

union linkroot {
	struct linklist list;
	struct linknode node;
};

extern char opts[];
extern HashTable shfunctab;
extern volatile long lastval;

extern LinkList newsizedlist(int size);
extern Param setaparam(char *s, char **aval);
extern Param sethparam(char *s, char **val);
extern Param setnparam(char *s, mnumber val);
extern Param setsparam(char *s, char *val);
extern char **featuresarray(Module m, Features f);
extern char **getaparam(char *s);
extern char **gethkparam(char *s);
extern char **gethparam(char *s);
extern char *dupstring(const char *s);
extern char *getsparam(char *s);
extern char *ztrdup(const char *s);
extern void
execstring(char *s, int dont_change_job, int exiting, char *context);
extern int dosetopt(int optno, int value, int force, char *new_opts);
extern int doshfunc(Shfunc shfunc, LinkList doshargs, int noreturnval);
extern int handlefeatures(Module m, Features f, int **enables);
extern int optlookup(char const *name);
extern int setfeatureenables(Module m, Features f, int *e);
extern mnumber getnparam(char *s);
extern void *zalloc(size_t size);
extern void popheap(void);
extern void pushheap(void);
extern void unsetparam(char *s);
extern void zsfree(char *p);

static inline LinkNode firstnode(LinkList list)
{
	return list->list.first;
}

static inline LinkNode lastnode(LinkList list)
{
	return list->list.last;
}

static inline LinkNode getsizednode(LinkList list, int index)
{
	return &firstnode(list)[index];
}

static inline void setsizednode(LinkList list, int index, void *dat)
{
	getsizednode(list, index)->dat = dat;
}
