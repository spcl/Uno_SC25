#!/bin/bash

sudo apt update
sudo apt install -y libgraphviz-dev

sudo apt install -y python3-pip
pip3 install plotly
pip3 install pandas
pip3 install natsort