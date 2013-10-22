/**
 * \file plots.h
 *
 * Utilities for generating plots.
 */

#ifndef __PLOTS_H__
#define __PLOTS_H__

#include <vector>
#include <string>

class TCanvas;
class TLegend;
class TNtuple;
class TH1;

/**
 * Convenience class for making 1D spectral plots.
 */
class SpectralPlot {
  public:
    /**
     * Constructor.
     *
     * \param _line_width Line width for histograms
     * \param _xmin Minimum of the x range
     * \param _xmax Maximum of the x range
     * \param _ymin Minimum of the y range
     * \param _ymax Maximum of the y range
     * \param _logy Set logscale on the y axis
     * \param _title Histogram title
     * \param _xtitle Title of x axis
     * \param _ytitle Title of y axis
     */
    SpectralPlot(int _line_width=2, float _xmin=1.5, float _xmax=5.0,
                 float _ymin=1e-1, float _ymax=1e3, bool _logy=true,
                 std::string _title="",
                 std::string _xtitle="",
                 std::string _ytitle="Counts/bin");

    /** Copy constructor. */
    SpectralPlot(const SpectralPlot& o);

    /** Destructor. */
    ~SpectralPlot();

    /**
     * Add a histogram to the plot.
     *
     * No reference to the input histogram is retained.
     *
     * \param _h The histogram
     * \param title Histogram title to show in the legend
     * \param options Options passed to TH1::Draw
     */
    void add(TH1* _h, std::string title, std::string options="");

    /**
     * Save the plot to a file, with type based on extension.
     * 
     * \param filename Name of the output file
     */
    void save(std::string filename);

    /**
     * Create an empty histogram with the same properties as the input.
     *
     * \param h The input histogram
     * \param name Name for the ROOT object
     * \returns An empty histogram, much like the input
     */
    static TH1* make_like(TH1* h, std::string name);

    bool logy;  //!< Log y axis on/off
    int line_width;  //!< Histogram line width
    float xmin;  //!< x-range minimum
    float xmax;  //!< x-range maximum
    float ymin;  //!< y-range minimum
    float ymax;  //!< y-range maximum
    std::string title;  //!< plot title
    std::string xtitle;  //!< x axis title
    std::string ytitle;  //!< y axis title
    TCanvas* c;  //!< Canvas to plot on
    std::vector<TH1*> histograms;  //!< Pointers to plotted histograms
    TLegend* legend;  //!< Plot legend
};

#endif  // __PLOTS_H__
