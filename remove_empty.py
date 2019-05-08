#!/usr/bin/env 
#coding: utf-8
from wrapper import clean_url, check_dump_folder, remove_dump_folder
import wrapper_config


if __name__ == '__main__':
    try:
        urls = open(wrapper_config.URLS_FILE).read().splitlines()
    except IOError:
        logging.info('cant open %s check file' % wrapper_config.URLS_FILE)
        sys.exit()
    for url in urls:
        if not check_dump_folder(clean_url(url)):
            print 'remove'
            remove_dump_folder(clean_url(url))
