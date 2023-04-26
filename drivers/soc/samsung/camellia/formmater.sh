#!/bin/bash

astyle ./*.c,*.h \
          --recursive \
          -t \
          --style=linux \
          --lineend=linux \
          --max-code-length=75 \
          --remove-braces \
          --indent=force-tab \
          --align-pointer=name
