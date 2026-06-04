#!/bin/bash
docker run -it --rm \
  -v $(pwd):/code \
  --network host \
  redis-scratch bash