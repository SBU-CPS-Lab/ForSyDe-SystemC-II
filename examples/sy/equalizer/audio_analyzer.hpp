/**********************************************************************
    * audio_analyzer.hpp -- analyzes the current bass level and raises*
    *                    a flag when the bass level exceeds a limit.  *
    *                                                                 *
    * Author:  Hosein Attarzadeh (h_attarzadeh@sbu.ac.ir)             *
    *          the Haskell version from Ingo Sander (ingo@kth.se)     *
    *                                                                 *
    * Purpose: Demonstrating how co-simulation with legacy codes is   *
    *          performed.                                             *
    *                                                                 *
    * Usage:   Equalizer example                                      *
    *                                                                 *
    * License: BSD3                                                   *
    *******************************************************************/
#ifndef AUDIO_ANALYZER_HPP
#define AUDIO_ANALYZER_HPP

#include <forsyde.hpp>
#include <globals.hpp>

#include <tuple>
#include <complex>
#include <numeric>

// grppts samples are grouped and transformed, so the DFT below produces
// exactly grppts bins. take_spectrum_func then reads bins 1..nLow --
// skipping bin 0, the DC component -- to measure the low end, so the
// transform has to produce at least nLow+1 bins for that to be in
// range.
//
// grppts was 2, which gives a DFT with only bins 0 and 1 while nLow=3
// asks for bins 1, 2 and 3. Every run walked off the end of the vector
// on its very first group and died on the libstdc++ bounds assertion
// ("__n < this->size()"); a 2-point DFT is also too degenerate to
// analyse an audio spectrum with at all. Raised to 8 -- a power of two,
// as a DFT window normally is, giving 7 non-DC bins for nLow=3 to draw
// from.
#define grppts 8
#define limit  1.0
#define nLow   3

// Keep the two in step: this relationship is what the indexing in
// take_spectrum_func depends on, and nothing else enforces it.
static_assert(grppts >= nLow + 1,
    "grppts must provide at least nLow+1 DFT bins, since take_spectrum_func "
    "reads bins 1..nLow and bin 0 is the DC component");

using namespace ForSyDe::SY;

void to_complex_func(std::complex<double>& out1, const double& inp1)
{
    out1 = std::complex<double>(inp1, 0);
}


void dft_func(abst_ext<std::vector<std::complex<double>>>& out,
                const abst_ext<std::vector<abst_ext<std::complex<double>>>> inp)
{
    if (is_absent(inp))
        out = abst_ext<std::vector<std::complex<double>>>();
    else
    {
        auto list = unsafe_from_abst_ext(inp);
        std::vector<std::complex<double>> temp;
        unsigned int n = list.size();
        for (unsigned int k = 0; k < n; ++k)
        {
            std::complex<double> result;
            for (unsigned int j = 0; j < n; ++j)
            {
                std::complex<double> omega(cos(k * j * 2*M_PI/n), sin(k * j * 2*M_PI/n));
                result += unsafe_from_abst_ext(list[j]) * omega;
            }
            temp.push_back(result);
        }
        out = abst_ext<std::vector<std::complex<double>>>(temp);
    }
}

void take_spectrum_func(abst_ext<std::vector<double>>& out, abst_ext<std::vector<std::complex<double>>> inp)
{
    if (is_absent(inp))
        out = abst_ext<std::vector<double>>();
    else
    {
        std::vector<std::complex<double>> tempvec = unsafe_from_abst_ext(inp);
        std::vector<double> retvec;
        for (int i=0;i<nLow;i++)
            retvec.push_back(log10(std::norm(tempvec[i+1])));
        out = abst_ext<std::vector<double>>(retvec);
    }
}

void check_bass_func(abst_ext<AnalyzerMsg>& out, abst_ext<std::vector<double>> inp)
{
    if (is_absent(inp))
        out = abst_ext<AnalyzerMsg>();
    else
    {
        std::vector<double> tempvec = unsafe_from_abst_ext(inp);
        double ta = std::accumulate(tempvec.begin(), tempvec.end(), 0.0);
        out = abst_ext<AnalyzerMsg>(ta > limit ? Fail : Pass);
    }
}

struct audio_analyzer : ForSyDe::composite
{
    SY::in_port<double> audioIn;

    SY::out_port<AnalyzerMsg> analyzerOut;

    SY::signal<std::complex<double>> cmplxSig;
    SY::signal<std::vector<abst_ext<std::complex<double>>>> grpSig;
    SY::signal<std::vector<std::complex<double>>> dftSig;
    SY::signal<std::vector<double>> spectrumSig;

    SC_CTOR(audio_analyzer)
    {
        add(new scomb("to_complex1", to_complex_func))(cmplxSig, audioIn);

        add(new group<std::complex<double>>("group_samples", grppts))(grpSig, cmplxSig);

        add(new comb("dft1", dft_func))(dftSig, grpSig);

        add(new comb("take_spectrum", take_spectrum_func))(spectrumSig, dftSig);

        add(new comb("check_bass", check_bass_func))(analyzerOut, spectrumSig);
    }
};

#endif
