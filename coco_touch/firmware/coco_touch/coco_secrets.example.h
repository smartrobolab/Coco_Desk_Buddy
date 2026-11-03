// Copy this file to coco_secrets.h (git ignores that one) and put your keys in it.
// coco_config.h includes coco_secrets.h when it exists; build.sh leaves it out of
// the public release files (PRIVATE=1 build.sh uses it).
#pragma once
#define OWM_API_KEY "your OpenWeatherMap API key"
