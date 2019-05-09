# WrapSQLMAP


WrapSQLMAP - reworked version of sqlmap for mass draining or vulnerability scan

1.Filter sites.

2.Brute CMS: WordPress, Drupal, OpenCart

Usage
----

If you need to scan sites using sqlmap, then specify in wrapper_config.py: Check_SQLi = True
If you need to merge conons, then specify in wrapper_config.py: DUMP = True, and when starting, specify the columns to be found:

    python wrapper.py email,pass,card

