
makeproto -osdmake-protos.h buffer.c convert.c main.c run.c cmdlist.c depend.c parse.c var.c subs.c
dcc -v -r -new -Odtmp:dmake -o sdmake buffer.c convert.c main.c run.c cmdlist.c depend.c parse.c var.c subs.c -s


