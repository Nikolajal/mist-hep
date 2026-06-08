// SPDX-License-Identifier: MIT
//
// mist/hep/hep.h — convenience umbrella that pulls in every public header.
//
// Prefer the targeted includes in real code; this header exists so consumers
// can `#include <mist/hep/hep.h>` from prototyping scripts without thinking.
//
#pragma once

#include "mist/hep/owned.h"
#include "mist/hep/stats.h"
#include "mist/hep/globals.h"
#include "mist/hep/fit/functions.h"
#include "mist/hep/graph/transforms.h"
#include "mist/hep/graph/binning.h"
#include "mist/hep/graph/smoothing.h"
#include "mist/hep/graph/algebra.h"
#include "mist/hep/histo/histo.h"
#include "mist/hep/histo/fill.h"
#include "mist/hep/signal/waveform.h"
