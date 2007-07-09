package edu.cmu.sphinx.tools.confdesigner.actions;

import javax.swing.*;
import java.awt.event.ActionEvent;
import java.awt.event.KeyEvent;

/**
 * DOCUMENT ME!
 *
 * @author Holger Brandl
 */
public class CopySubGraphAction extends AbstractAction {

    public CopySubGraphAction() {
        putValue(NAME, "Copy");
        putValue(ACCELERATOR_KEY, KeyStroke.getKeyStroke(KeyEvent.VK_C, KeyEvent.CTRL_MASK));
        putValue(MNEMONIC_KEY, KeyEvent.VK_C);
    }


    public void actionPerformed(ActionEvent e) {

    }
}
