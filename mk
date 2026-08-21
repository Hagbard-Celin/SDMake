
makeproto -osdmake-protos.h buffer.c convert.c main.c run.c cmdlist.c depend.c parse.c var.c path.c system13.c console.c cond.c string.c parserevh.c
dcc -v -r -new -Odtmp:dmake -o sdmake buffer.c convert.c main.c run.c cmdlist.c depend.c parse.c var.c subs.c -s


