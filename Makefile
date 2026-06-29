# Kill Grok Orphans — built with Grok16 (g16)
GROK16_ROOT ?= /home/default/Desktop/SG/Grok16
G16         ?= $(GROK16_ROOT)/bin/g16
PREFIX      ?= /usr/local
VERSION     := $(shell cat VERSION 2>/dev/null || echo 1.0.0)

CFLAGS  := -std=gnu17 -O2 -Wall -Wextra -fno-pie -D_GNU_SOURCE -DKGO_VERSION=\"$(VERSION)\"
LDFLAGS := -no-pie -static

SRCS := src/kgo.c src/kgo_scan.c src/kgo_config.c
OBJS := $(SRCS:.c=.o)

.PHONY: all clean install package

all: bin/kgo

bin/kgo: $(OBJS)
	@mkdir -p bin
	$(G16) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

$(OBJS): src/kgo.h

src/%.o: src/%.c
	$(G16) $(CFLAGS) -c -o $@ $<

install: bin/kgo
	install -d $(DESTDIR)$(PREFIX)/sbin
	install -d $(DESTDIR)/etc/kgo
	install -m 755 bin/kgo $(DESTDIR)$(PREFIX)/sbin/kgo
	install -m 644 data/kgo-patterns.json $(DESTDIR)/etc/kgo/kgo-patterns.json

package: all
	@mkdir -p dist
	tar -czf dist/kgo-$(VERSION)-linux-x86_64.tar.gz \
		bin/kgo data/kgo-patterns.json python/kgo_watchdog.py \
		packaging/linux/install.sh packaging/linux/kgo.service \
		README.md LICENSE VERSION

clean:
	rm -f $(OBJS) bin/kgo
	rm -rf dist