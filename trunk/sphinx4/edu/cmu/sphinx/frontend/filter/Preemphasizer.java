/*
 * Copyright 1999-2004 Carnegie Mellon University.  
 * Portions Copyright 2002-2004 Sun Microsystems, Inc.  
 * Portions Copyright 2002-2004 Mitsubishi Electronic Research Laboratories.
 * All Rights Reserved.  Use is subject to license terms.
 * 
 * See the file "license.terms" for information on usage and
 * redistribution of this file, and for a DISCLAIMER OF ALL 
 * WARRANTIES.
 *
 */

package edu.cmu.sphinx.frontend.filter;

import edu.cmu.sphinx.frontend.BaseDataProcessor;
import edu.cmu.sphinx.frontend.Data;
import edu.cmu.sphinx.frontend.DataProcessingException;
import edu.cmu.sphinx.frontend.DataProcessor;
import edu.cmu.sphinx.frontend.DataEndSignal;
import edu.cmu.sphinx.frontend.DoubleData;

import edu.cmu.sphinx.util.SphinxProperties;
import edu.cmu.sphinx.util.Timer;

import java.io.IOException;


/**
 * Implements a high-pass filter that filters out the attenuation 
 * of audio data. Speech signals have an attenuation (a decrease in
 * intensity of a signal) of 20 dB/dec. It increases the magnitude
 * of the higher frequencies with respect to the lower frequencies.
 * One of the goals of preemphasis is to reduce background noise,
 * and increase the signal-to-noise ratio. It is
 * a high-pass filter because it emphasizes the high frequency components,
 * and de-emphasizes the low frequency components. High-pass filters
 * are so called because the allow the high frequency components to "pass 
 * through", while weakening or filtering out the low frequency components.
 *
 * The Preemphasizer takes a Data object that usually represents
 * audio data as input, and outputs the same Data object, but with
 * preemphasis applied. For each value X[i] in the input Data object X,
 * the following formula is applied to obtain the output Data object Y:
 * <p>
 * <code>
 * Y[i] = X[i] - (X[i-1] * preemphasisFactor)
 * </code>
 * <p>
 * (note that 'i' is time)
 * <p>
 * The preemphasis factor normally has a value of 0.97. 
 * <p>
 * The figure below shows an signal plotted in time:
 * <p>
 * The figure below shows the corresponding preemphasized signal
 * plotted in time:
 * <p>
 * The relevant SphinxProperty for this Preemphasizer is:
 * <pre>
 * edu.cmu.sphinx.frontend.filter.Preemphasizer.factor
 * </pre>
 *
 * Other Data objects are passed along unchanged through this Preemphasizer.
 */
public class Preemphasizer extends BaseDataProcessor {

    /**
     * The name of the SphinxProperty for preemphasis factor/alpha, which
     * has a default value of 0.97F.
     */
    public static final String PROP_PREEMPHASIS_FACTOR = 
	"edu.cmu.sphinx.frontend.window.Preemphasizer.factor";

    /**
     * The default value of PROP_PREEMPHASIS_FACTOR.
     */
    public static final double PROP_PREEMPHASIS_FACTOR_DEFAULT = 0.97;
    
    
    private double preemphasisFactor;
    private double prior;
    private Timer timer;
    

    /**
     * Constructs a default Preemphasizer.
     *
     * @param name      the name of this Preemphasizer
     * @param frontEnd  the name of the front end this DataProcessor belongs
     * @param props     the SphinxProperties object to use
     * @param predecessor the DataProcessor from which it obtains Data objects
     */
    public void initialize(String name, String frontEnd,
                           SphinxProperties props,
                           DataProcessor predecessor) {
        super.initialize((name == null ? "Preemphasizer" : name),
                         frontEnd, props, predecessor);
        this.preemphasisFactor = getSphinxProperties().getDouble
	    (getFullPropertyName(PROP_PREEMPHASIS_FACTOR),
             PROP_PREEMPHASIS_FACTOR_DEFAULT);
    }


    /**
     * Returns the next Data object being processed by this Preemphasizer,
     * or if it is a Signal, it is returned without modification.
     *
     * @return the next available Data object, returns null if no
     *     Data object is available
     *
     * @throws DataProcessingException if there is a processing error
     *
     * @see Data
     */
    public Data getData() throws DataProcessingException {

	Data input = getPredecessor().getData();

        getTimer().start();
        
        if (input != null) {
            if (input instanceof DoubleData) {
                applyPreemphasis(((DoubleData) input).getValues());
            } else if (input instanceof DataEndSignal) {
                prior = 0;
            }
        }

        getTimer().stop();

        return input;
    }	


    /**
     * Applies pre-emphasis filter to the given Audio. The preemphasis
     * is applied in place.
     *
     * @param in   audio data
     */
    private void applyPreemphasis(double[] in) {

        // set the prior value for the next Audio
        double nextPrior = prior;
        if (in.length > 0) {
            nextPrior = in[in.length - 1];
        }

	if (in.length > 1 && preemphasisFactor != 0.0) {
	    
            // do preemphasis
            double current;
            double previous = in[0];
            in[0] = previous - preemphasisFactor * prior;

	    for (int i = 1; i < in.length; i++) {
                current = in[i];
		in[i] = current - preemphasisFactor * previous;
                previous = current;
	    }
	}

        prior = nextPrior;
    }
}
