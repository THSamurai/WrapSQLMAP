#!/usr/bin/env python
#coding: utf-8
from wrapper import clean_url, DUMP_SQLMAP_FOLDER, check_dump_folder, make_txt_dump, remove_dump_folder
from shutil import rmtree
import wrapper_config


if __name__ == '__main__':
    try:
        urls = open(wrapper_config.URLS_FILE).read().splitlines()
    except IOError:
        logging.info('cant open %s check file' % wrapper_config.URLS_FILE)
        sys.exit()
    for url in urls:
        if check_dump_folder(clean_url(url)):
            make_txt_dump(clean_url(url))
        remove_dump_folder(clean_url(url))
    rmtree(DUMP_SQLMAP_FOLDER)