export MANGOH_ROOT=$(shell pwd)/../../../..

all: wakeupAndTweet.wp85.update wakeupAndTweet.zip

wakeupAndTweet.wp85.update:
	mkapp -t wp85 \
            --source-search=../components \
            --interface-search=$(LEGATO_ROOT)/interfaces/modemServices \
            --interface-search=$(MANGOH_ROOT)/apps/SocialService/interfaces \
            wakeupAndTweet.adef

wakeupAndTweet.zip: wakeupAndTweet.wp85.update _build_wakeupAndTweet
	av-pack -u wakeupAndTweet.wp85.update -b _build_wakeupAndTweet

clean:
	rm -rf wakeupAndTweet.zip wakeupAndTweet.wp85.update _build_wakeupAndTweet
