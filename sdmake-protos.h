
/* MACHINE GENERATED */


/* buffer.c             */

Prototype char *AllocPathBuffer(void);
Prototype void FreePathBuffer(char *);

/* convert.c            */

Prototype int WildConvert(char *srcBuf, List *dstList, char *srcMat, char *dstMat);

/* main.c               */

Prototype void MemErr(void);
Prototype List	DoList;
Prototype short DDebug;
Prototype short CacheLevel;
Prototype short NoRunOpt;
Prototype short QuietOpt;
Prototype short QuietCmd;
Prototype short CheckTarget;
Prototype short ExitCode;
Prototype short	DoAll;
Prototype short SomeWork;
Prototype APTR  MemPool;

/* run.c                */

Prototype long Execute_Command(char *cmd, short ignore, short quiet, IfNode **cmdIfBase, LONG *cmdIfTrue);
Prototype void InitCommand(void);

/* cmdlist.c            */

Prototype void InitCmdList(void);
Prototype void PutCmdListChar(List *, char);
Prototype void PutCmdListSym(List *, char *, short *);
Prototype void PutCmdListLen(List *list, char *buf, LONG len);
Prototype void CopyCmdList(List *, List *);
Prototype void FreeCmdList(List *);
Prototype void AppendCmdList(List *, List *);
Prototype WORD PopCmdListSym(List *, char *, WORD);
Prototype int  PopCmdListChar(List *);
Prototype void CopyCmdListBuf(List *, char *);
Prototype void CopyCmdListNewLineBuf(List *, char *);
Prototype long CmdListSize(List *);
Prototype long CmdListSizeNewLine(List *);
Prototype void CopyCmdListConvert(List *, List *, char *, char *);
Prototype long ExecuteCmdList(DepNode *, List *);

/* depend.c             */

Prototype void InitDep(void);
Prototype DepRef  *CreateDepRef(List *, CONST_STRPTR);
Prototype DepCmdList *AllocDepCmdList(void);
Prototype DepRef  *DupDepRef(DepRef *);
Prototype void	  IncorporateDependency(DepRef *, DepRef *, List *);
Prototype int ExecuteDependency(DepNode *parent, DepRef *lhs);
Prototype List DepList;

/* parse.c              */

Prototype void InitParser(void);
Prototype void ParseFile(STRPTR);
Prototype token_t XGetElement(void);
Prototype STRPTR ExpandVariable(STRPTR, List *);
Prototype void error(short type, CONST_STRPTR ctl, ...);
Prototype char SymBuf[256];
Prototype long LineNo;
Prototype char SpecialChar[256];

/* var.c                */

Prototype void InitVariable(void);
Prototype Var *MakeVariable(char *, char);
Prototype Var *FindVariable(char *, char);
Prototype void AppendVariable(Var *, char *, long);

/* path.c               */

Prototype long _SearchPath(char *cmd);

/* system13.c           */

Prototype int system13(const char *buf);

/* console.c            */

Prototype BOOL OpenConsole(const char *str);

/* cond.c               */

Prototype int pushIf(IfNode **ifBase, int value);
Prototype int popIf(IfNode **ifBase);
Prototype int elseIf(IfNode **ifBase);

/* string.c             */

Prototype BOOL StriInStr(CONST_STRPTR find, CONST_STRPTR string);

/* parserevh.c          */

Prototype WORD ParseRevInclude(STRPTR includefile);
