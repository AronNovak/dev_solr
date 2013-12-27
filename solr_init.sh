#!/bin/bash
# Initializes local multi-core Solr environment
ORIG_DIR=$PWD
SOLR_VER="3.6.2"

wget -O /tmp/solr.zip http://archive.apache.org/dist/lucene/solr/3.6.2/apache-solr-$SOLR_VER.zip
unzip /tmp/solr.zip -d /opt
rm /tmp/solr.zip
cd /opt/apache-solr-$SOLR_VER
mv example dev
cd dev/multicore
rm -rf core0
rm -rf core1
mv $ORIG_DIR/empty_solr.xml solr.xml
cd $ORIG_DIR

