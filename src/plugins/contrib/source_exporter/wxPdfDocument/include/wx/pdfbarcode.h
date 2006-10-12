///////////////////////////////////////////////////////////////////////////////
// Name:        pdfbarcode.h
// Purpose:     
// Author:      Ulrich Telle
// Modified by:
// Created:     2005-09-12
// Copyright:   (c) Ulrich Telle
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

/// \file pdfbarcode.h Interface of the wxPdfBarCodeCreator class

#ifndef _PDFBARCODE_H_
#define _PDFBARCODE_H_

//#if defined(__GNUG__) && !defined(__APPLE__)
//    #pragma interface "pdfbarcode.h"
//#endif

#include "wx/pdfdocdef.h"

// Forward declarations

class WXDLLIMPEXP_PDFDOC wxPdfDocument;

/// Class representing barcode objects.
/**
* All supported barcodes are drawn directly in PDF without using an image or special font.
*/
class WXDLLIMPEXP_PDFDOC wxPdfBarCodeCreator
{
public:
  /// Constructor
  /**
  * \param document associated wxPdfDocument instance
  */
  wxPdfBarCodeCreator(wxPdfDocument& document);

  /// Destructor
  virtual ~wxPdfBarCodeCreator();

  /// Draw a EAN13 barcode
  /**
  * An EAN13 barcode is made up of 13 digits,
  * The last digit is a check digit; if it's not supplied, it will be automatically computed.
  * \param x abscissa of barcode
  * \param y ordinate of barcode
  * \param barcode value of barcode
  * \param h height of barcode. Default value: 16
  * \param w width of a bar. Default value: 0.35.
  * \returns TRUE if barcode could be drawn, FALSE if the check digit is invalid
  */
  bool EAN13(double x, double y, const wxString& barcode, double h = 16, double w = .35);

  /// Draw a UPC-A barcode
  /**
  * An UPC-A barcode is made up of 12 digits (leading zeroes are added if necessary).
  * The last digit is a check digit; if it's not supplied, it will be automatically computed.
  * \param x abscissa of barcode
  * \param y ordinate of barcode
  * \param barcode value of barcode
  * \param h height of barcode. Default value: 16
  * \param w width of a bar. Default value: 0.35.
  * \returns TRUE if barcode could be drawn, FALSE if the check digit is invalid
  */
  bool UPC_A(double x, double y, const wxString& barcode, double h = 16, double w = .35);

  /// Draw standard or extended Code39 barcode
  /**
  * This method supports both standard and extended Code 39 barcodes.
  * The extended mode gives access to the full ASCII range (from 0 to 127).
  * It is also possible to add a checksum.
  * \param x: abscissa
  * \param y: ordinate
  * \param code: barcode value
  * \param ext: indicates if extended mode must be used (true by default)
  * \param cks: indicates if a checksum must be appended (false by default)
  * \param w: width of a narrow bar (0.4 by default)
  * \param h: height of bars (20 by default)
  * \param wide: indicates if ratio between wide and narrow bars is high; if yes, ratio is 3, if no, it's 2 (true by default) 
  */
  bool Code39(double x, double y, const wxString& code, bool ext = true, bool cks = false, double w = 0.4, double h = 20, bool wide = true);

  /// Draw an Interleaved 2 of 5 barcode
  /**
  * An Interleaved 2 of 5 barcode contains digits (0 to 9) and encodes the
  * data in the width of both bars and spaces. It is used primarily in the
  * distribution and warehouse industry.
  * \param xpos: abscissa of barcode
  * \param ypos: ordinate of barcode
  * \param code: value of barcode (Note: if the length of the code is not even, a 0 is preprended.)
  * \param basewidth: corresponds to the width of a wide bar (defaults to 1)
  * \param height: bar height (defaults to 10)
  */
  bool I25(double xpos, double ypos, const wxString& code, double basewidth = 1, double height = 10);

  /// Draw U.S. Postal Service POSTNET barcodes
  /**
  * This method supports both 5 and 9 digit zip codes. 
  * Zipcode must be a string containing a zip code of the form DDDDD or DDDDD-DDDD.
  * \param x: abscissa of barcode
  * \param y: ordinate of barcode
  * \param zipcode: zip code to draw
  * \returns TRUE if barcode could be drawn, FALSE if the zipcode is invalid
  */
  bool PostNet(double x, double y, const wxString& zipcode);

protected:
  /// Calculate check digit
  wxChar GetCheckDigit(const wxString& barcode);

  /// Validate check digit
  bool TestCheckDigit(const wxString& barcode);

  /// Draw a barcode
  bool Barcode(double x, double y, const wxString& barcode, double h, double w, int len);

  /// Encode extended Code39 barcode
  wxString EncodeCode39Ext(const wxString& code);

  /// Calculate Code39 check sum
  wxChar ChecksumCode39(const wxString& code);

  /// Draw Code39 barcode
  void DrawCode39(const wxString& code, double x, double y, double w, double h);

  /// Validate ZIP code
  bool ZipCodeValidate(const wxString& zipcode);

  /// Calculate ZIP code check sum digit
  int ZipCodeCheckSumDigit(const wxString& zipcode);

  /// Draw ZIP code barcode
  void ZipCodeDrawDigitBars(double x, double y, double barSpacing,
                            double halfBarHeight, double fullBarHeight, int digit);

private:
  wxPdfDocument* m_document;  ///< Document this barcode creator belongs to
};

#endif
