#ifndef SUPERLETS_H
#define SUPERLETS_H

#include "analysismethod.h"
#include "helpers/analysisIO.h"
#include "helpers/morlet.h"

namespace TIMEFREQUENCYLIB {

struct SuperletSettings{
    AnalysisSettings settings;
    int		freq_count;
    int		input_size;
    float	wavelet_cycles;
    float	resolution_low;
    float	resolution_high;
};

class Superlets : public AnalysisMethod
{
public:
    Superlets();

    virtual bool compute();
    virtual bool validateSettings();
};
} //namespace

#endif // SUPERLETS_H
