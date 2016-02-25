/* poppler-page.cc: qt interface to poppler
 * Copyright (C) 2005, Net Integration Technologies, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <poppler-qt4.h>

#include <QtCore/QMap>
#include <QtCore/QVarLengthArray>
#include <QtGui/QImage>
#include <QtGui/QPainter>

#include <config.h>
#include <PDFDoc.h>
#include <Catalog.h>
#include <Form.h>
#include <ErrorCodes.h>
#include <TextOutputDev.h>
#if defined(HAVE_SPLASH)
#include <SplashOutputDev.h>
#include <splash/SplashBitmap.h>
#include <ArthurOutputDev.h>
#endif

#include "poppler-private.h"
#include "poppler-page-transition-private.h"
#include "poppler-page-private.h"
#include "poppler-link-extractor-private.h"
#include "poppler-annotation-helper.h"
#include "poppler-annotation-private.h"
#include "poppler-form.h"

namespace Poppler {

class DummyAnnotation : public Annotation
{
    public:
        DummyAnnotation()
            : Annotation( *new AnnotationPrivate() )
        {
        }

        virtual SubType subType() const { return A_BASE; }
};

Link* PageData::convertLinkActionToLink(::LinkAction * a, const QRectF &linkArea)
{
  if ( !a )
    return NULL;

  Link * popplerLink = NULL;
  switch ( a->getKind() )
  {
    case actionGoTo:
    {
      LinkGoTo * g = (LinkGoTo *) a;
      // create link: no ext file, namedDest, object pointer
      popplerLink = new LinkGoto( linkArea, QString::null, LinkDestination( LinkDestinationData(g->getDest(), g->getNamedDest(), parentDoc ) ) );
    }
    break;

    case actionGoToR:
    {
      LinkGoToR * g = (LinkGoToR *) a;
      // copy link file
      const char * fileName = g->getFileName()->getCString();
      // ceate link: fileName, namedDest, object pointer
      popplerLink = new LinkGoto( linkArea, (QString)fileName, LinkDestination( LinkDestinationData(g->getDest(), g->getNamedDest(), parentDoc ) ) );
    }
    break;

    case actionLaunch:
    {
      LinkLaunch * e = (LinkLaunch *)a;
      GooString * p = e->getParams();
      popplerLink = new LinkExecute( linkArea, e->getFileName()->getCString(), p ? p->getCString() : 0 );
    }
    break;

    case actionNamed:
    {
      const char * name = ((LinkNamed *)a)->getName()->getCString();
      if ( !strcmp( name, "NextPage" ) )
        popplerLink = new LinkAction( linkArea, LinkAction::PageNext );
      else if ( !strcmp( name, "PrevPage" ) )
        popplerLink = new LinkAction( linkArea, LinkAction::PagePrev );
      else if ( !strcmp( name, "FirstPage" ) )
        popplerLink = new LinkAction( linkArea, LinkAction::PageFirst );
      else if ( !strcmp( name, "LastPage" ) )
        popplerLink = new LinkAction( linkArea, LinkAction::PageLast );
      else if ( !strcmp( name, "GoBack" ) )
        popplerLink = new LinkAction( linkArea, LinkAction::HistoryBack );
      else if ( !strcmp( name, "GoForward" ) )
        popplerLink = new LinkAction( linkArea, LinkAction::HistoryForward );
      else if ( !strcmp( name, "Quit" ) )
        popplerLink = new LinkAction( linkArea, LinkAction::Quit );
      else if ( !strcmp( name, "GoToPage" ) )
        popplerLink = new LinkAction( linkArea, LinkAction::GoToPage );
      else if ( !strcmp( name, "Find" ) )
        popplerLink = new LinkAction( linkArea, LinkAction::Find );
      else if ( !strcmp( name, "FullScreen" ) )
        popplerLink = new LinkAction( linkArea, LinkAction::Presentation );
      else if ( !strcmp( name, "Close" ) )
      {
        // acroread closes the document always, doesnt care whether 
        // its presentation mode or not
        // popplerLink = new LinkAction( linkArea, LinkAction::EndPresentation );
        popplerLink = new LinkAction( linkArea, LinkAction::Close );
      }
      else
      {
        // TODO
      }
    }
    break;

    case actionURI:
    {
      popplerLink = new LinkBrowse( linkArea, ((LinkURI *)a)->getURI()->getCString() );
    }
    break;

    case actionSound:
    {
      ::LinkSound *ls = (::LinkSound *)a;
      popplerLink = new LinkSound( linkArea, ls->getVolume(), ls->getSynchronous(), ls->getRepeat(), ls->getMix(), new SoundObject( ls->getSound() ) );
    }
    break;

    case actionMovie:
/*      TODO this (Movie link)
          m_type = Movie;
          LinkMovie * m = (LinkMovie *) a;
          // copy Movie parameters (2 IDs and a const char *)
          Ref * r = m->getAnnotRef();
          m_refNum = r->num;
          m_refGen = r->gen;
          copyString( m_uri, m->getTitle()->getCString() );
*/  break;

    case actionUnknown:
    break;
  }

  return popplerLink;
}


Page::Page(DocumentData *doc, int index) {
  m_page = new PageData();
  m_page->index = index;
  m_page->parentDoc = doc;
  m_page->page = doc->doc->getCatalog()->getPage(m_page->index + 1);
  m_page->transition = 0;
}

Page::~Page()
{
  delete m_page->transition;
  delete m_page;
}

QImage Page::renderToImage(double xres, double yres, int x, int y, int w, int h, Rotation rotate) const
{
  int rotation = (int)rotate * 90;
  QImage img;
  switch(m_page->parentDoc->m_backend)
  {
    case Poppler::Document::SplashBackend:
    {
#if defined(HAVE_SPLASH)
      SplashOutputDev *splash_output = static_cast<SplashOutputDev *>(m_page->parentDoc->getOutputDev());

      m_page->parentDoc->doc->displayPageSlice(splash_output, m_page->index + 1, xres, yres,
						 rotation, false, true, false, x, y, w, h);

      SplashBitmap *bitmap = splash_output->getBitmap();
      int bw = bitmap->getWidth();
      int bh = bitmap->getHeight();

      SplashColorPtr dataPtr = splash_output->getBitmap()->getDataPtr();

      if (QSysInfo::BigEndian == QSysInfo::ByteOrder)
      {
        uchar c;
        int count = bw * bh * 4;
        for (int k = 0; k < count; k += 4)
        {
          c = dataPtr[k];
          dataPtr[k] = dataPtr[k+3];
          dataPtr[k+3] = c;

          c = dataPtr[k+1];
          dataPtr[k+1] = dataPtr[k+2];
          dataPtr[k+2] = c;
        }
      }

      // construct a qimage SHARING the raw bitmap data in memory
      QImage tmpimg( dataPtr, bw, bh, QImage::Format_ARGB32 );
      img = tmpimg.copy();
      // unload underlying xpdf bitmap
      splash_output->startPage( 0, NULL );
#endif
      break;
    }
    case Poppler::Document::ArthurBackend:
    {
#if defined(HAVE_SPLASH)
      QSize size = pageSize();
      QImage tmpimg(w == -1 ? qRound( size.width() * xres / 72.0 ) : w, h == -1 ? qRound( size.height() * yres / 72.0 ) : h, QImage::Format_ARGB32);

      QPainter painter(&tmpimg);
      if (m_page->parentDoc->m_hints & Document::Antialiasing)
          painter.setRenderHint(QPainter::Antialiasing);
      if (m_page->parentDoc->m_hints & Document::TextAntialiasing)
          painter.setRenderHint(QPainter::TextAntialiasing);
      painter.save();
      painter.translate(x == -1 ? 0 : -x, y == -1 ? 0 : -y);
      ArthurOutputDev arthur_output(&painter);
      arthur_output.startDoc(m_page->parentDoc->doc->getXRef());
      m_page->parentDoc->doc->displayPageSlice(&arthur_output,
						 m_page->index + 1,
						 xres,
						 yres,
						 rotation,
						 false,
						 true,
						 false,
						 x,
						 y,
						 w,
						 h);
      painter.restore();
      painter.end();
      img = tmpimg;
#endif
      break;
    }
  }

  return img;
}

QString Page::text(const QRectF &r) const
{
  TextOutputDev *output_dev;
  GooString *s;
  PDFRectangle *rect;
  QString result;
  
  output_dev = new TextOutputDev(0, gFalse, gFalse, gFalse);
  m_page->parentDoc->doc->displayPageSlice(output_dev, m_page->index + 1, 72, 72,
      0, false, true, false, -1, -1, -1, -1);
  if (r.isNull())
  {
    rect = m_page->page->getCropBox();
    s = output_dev->getText(rect->x1, rect->y1, rect->x2, rect->y2);
  }
  else
  {
    double height, y1, y2;
    height = m_page->page->getCropHeight();
    y1 = height - r.top();
    y2 = height - r.bottom();
    s = output_dev->getText(r.left(), y1, r.right(), y2);
  }

  result = QString::fromUtf8(s->getCString());

  delete output_dev;
  delete s;
  return result;
}

bool Page::search(const QString &text, QRectF &rect, SearchDirection direction, SearchMode caseSensitive, Rotation rotate) const
{
  const QChar * str = text.unicode();
  int len = text.length();
  QVector<Unicode> u(len);
  for (int i = 0; i < len; ++i) u[i] = str[i].unicode();

  GBool sCase;
  if (caseSensitive == CaseSensitive) sCase = gTrue;
  else sCase = gFalse;

  bool found = false;
  double sLeft, sTop, sRight, sBottom;
  sLeft = rect.left();
  sTop = rect.top();
  sRight = rect.right();
  sBottom = rect.bottom();

  int rotation = (int)rotate * 90;

  // fetch ourselves a textpage
  TextOutputDev td(NULL, gTrue, gFalse, gFalse);
  m_page->parentDoc->doc->displayPage( &td, m_page->index + 1, 72, 72, rotation, false, true, false );
  TextPage *textPage=td.takeText();

  if (direction == FromTop)
    found = textPage->findText( u.data(), len, 
            gTrue, gTrue, gFalse, gFalse, sCase, gFalse, &sLeft, &sTop, &sRight, &sBottom );
  else if ( direction == NextResult )
    found = textPage->findText( u.data(), len, 
            gFalse, gTrue, gTrue, gFalse, sCase, gFalse, &sLeft, &sTop, &sRight, &sBottom );
  else if ( direction == PreviousResult )
    found = textPage->findText( u.data(), len, 
            gTrue, gFalse, gFalse, gTrue, sCase, gFalse, &sLeft, &sTop, &sRight, &sBottom );

  delete textPage;

  rect.setLeft( sLeft );
  rect.setTop( sTop );
  rect.setRight( sRight );
  rect.setBottom( sBottom );

  return found;
}

QList<TextBox*> Page::textList(Rotation rotate) const
{
  TextOutputDev *output_dev;
  
  QList<TextBox*> output_list;
  
  output_dev = new TextOutputDev(0, gFalse, gFalse, gFalse);
  
  int rotation = (int)rotate * 90;

  m_page->parentDoc->doc->displayPageSlice(output_dev, m_page->index + 1, 72, 72,
      rotation, false, false, false, -1, -1, -1, -1);

  TextWordList *word_list = output_dev->makeWordList();
  
  if (!word_list) {
    delete output_dev;
    return output_list;
  }
  
  QMap<TextWord *, TextBox*> wordBoxMap;
  
  for (int i = 0; i < word_list->getLength(); i++) {
    TextWord *word = word_list->get(i);
    GooString *gooWord = word->getText();
    QString string = QString::fromUtf8(gooWord->getCString());
    delete gooWord;
    double xMin, yMin, xMax, yMax;
    word->getBBox(&xMin, &yMin, &xMax, &yMax);
    
    TextBox* text_box = new TextBox(string, QRectF(xMin, yMin, xMax-xMin, yMax-yMin));
    text_box->m_data->hasSpaceAfter = word->hasSpaceAfter() == gTrue;
    text_box->m_data->charBBoxes.reserve(word->getLength());
    for (int j = 0; j < word->getLength(); ++j)
    {
        word->getCharBBox(j, &xMin, &yMin, &xMax, &yMax);
        text_box->m_data->charBBoxes.append(QRectF(xMin, yMin, xMax-xMin, yMax-yMin));
    }
    
    wordBoxMap.insert(word, text_box);
    
    output_list.append(text_box);
  }
  
  for (int i = 0; i < word_list->getLength(); i++) {
    TextWord *word = word_list->get(i);
    TextBox* text_box = wordBoxMap.value(word);
    text_box->m_data->nextWord = wordBoxMap.value(word->nextWord());
  }
  
  delete word_list;
  delete output_dev;
  
  return output_list;
}

PageTransition *Page::transition() const
{
  if (!m_page->transition) {
    Object o;
    PageTransitionParams params;
    params.dictObj = m_page->page->getTrans(&o);
    if (params.dictObj->isDict()) m_page->transition = new PageTransition(params);
    o.free();
  }
  return m_page->transition;
}

Link *Page::action( PageAction act ) const
{
  if ( act == Page::Opening || act == Page::Closing )
  {
    Object o;
    m_page->page->getActions(&o);
    if (!o.isDict())
    {
      o.free();
      return 0;
    }
    Dict *dict = o.getDict();
    Object o2;
    const char *key = act == Page::Opening ? "O" : "C";
    dict->lookup((char*)key, &o2);
    ::LinkAction *act = ::LinkAction::parseAction(&o2, m_page->parentDoc->doc->getCatalog()->getBaseURI() );
    o2.free();
    o.free();
    Link *popplerLink = NULL;
    if (act != NULL)
    {
      popplerLink = m_page->convertLinkActionToLink(act, QRectF());
      delete act;
    }
    return popplerLink;
  }
  return 0;
}

QSizeF Page::pageSizeF() const
{
  Page::Orientation orient = orientation();
  if ( ( Page::Landscape == orient ) || (Page::Seascape == orient ) ) {
      return QSizeF( m_page->page->getCropHeight(), m_page->page->getCropWidth() );
  } else {
    return QSizeF( m_page->page->getCropWidth(), m_page->page->getCropHeight() );
  }
}

QSize Page::pageSize() const
{
  return pageSizeF().toSize();
}

Page::Orientation Page::orientation() const
{
  const int rotation = m_page->page->getRotate();
  switch (rotation) {
  case 90:
    return Page::Landscape;
    break;
  case 180:
    return Page::UpsideDown;
    break;
  case 270:
    return Page::Seascape;
    break;
  default:
    return Page::Portrait;
  }
}

void Page::defaultCTM(double *CTM, double dpiX, double dpiY, int rotate, bool upsideDown)
{
  m_page->page->getDefaultCTM(CTM, dpiX, dpiY, rotate, gFalse, upsideDown);
}

QList<Link*> Page::links() const
{
  LinkExtractorOutputDev link_dev(m_page);
  m_page->parentDoc->doc->processLinks(&link_dev, m_page->index + 1);
  QList<Link*> popplerLinks = link_dev.links();

  return popplerLinks;
}

QList<Annotation*> Page::annotations() const
{
    Object annotArray;
    ::Page *pdfPage = m_page->page;
    pdfPage->getAnnots( &annotArray );
    if ( !annotArray.isArray() || annotArray.arrayGetLength() < 1 )
    {
        annotArray.free();
        return QList<Annotation*>();
    }

    // ID to Annotation/PopupWindow maps
    QMap< int, Annotation * > annotationsMap;
    QMap< int, PopupWindow * > popupsMap;
    // lists of Windows and Revisions that needs resolution
    QLinkedList< ResolveRevision > resolveRevList;
    QLinkedList< ResolveWindow > resolvePopList;
    QLinkedList< PostProcessText > ppTextList;

    // build a normalized transform matrix for this page at 100% scale
    GfxState * gfxState = new GfxState( 72.0, 72.0, pdfPage->getMediaBox(), pdfPage->getRotate(), gTrue );
    double * gfxCTM = gfxState->getCTM();
    double MTX[6];
    for ( int i = 0; i < 6; i+=2 )
    {
        MTX[i] = gfxCTM[i] / pdfPage->getCropWidth();
        MTX[i+1] = gfxCTM[i+1] / pdfPage->getCropHeight();
    }
    delete gfxState;

    /** 1 - PARSE ALL ANNOTATIONS AND POPUPS FROM THE PAGE */
    Object annot;
    Object annotRef;    // no need to free this (impl. dependent!)
    uint numAnnotations = annotArray.arrayGetLength();
    for ( uint j = 0; j < numAnnotations; j++ )
    {
        // get the j-th annotation
        annotArray.arrayGet( j, &annot );
        if ( !annot.isDict() )
        {
            qDebug() << "PDFGenerator: annot not dictionary.";
            annot.free();
            continue;
        }

        Annotation * annotation = 0;
        Dict * annotDict = annot.getDict();
        int annotID = annotArray.arrayGetNF( j, &annotRef )->getRefNum();
        bool parseMarkup = true,    // nearly all supported annots are markup
             addToPage = true;      // Popup annots are added to custom queue

        /** 1.1. GET Subtype */
        QString subType;
        XPDFReader::lookupName( annotDict, "Subtype", subType );
        if ( subType.isEmpty() )
        {
            qDebug() << "Annot has no Subtype";
            annot.free();
            continue;
        }

        /** 1.2. CREATE Annotation / PopupWindow and PARSE specific params */
        if ( subType == "Text" || subType == "FreeText" )
        {
            // parse TextAnnotation params
            TextAnnotation * t = new TextAnnotation();
            annotation = t;

            if ( subType == "Text" )
            {
                // -> textType
                t->setTextType( TextAnnotation::Linked );
                // -> textIcon
                QString tmpstring;
                XPDFReader::lookupName( annotDict, "Name", tmpstring );
                if ( !tmpstring.isEmpty() )
                {
                    tmpstring = tmpstring.toLower();
                    tmpstring.remove( ' ' );
                    t->setTextIcon( tmpstring );
                }
                // request for postprocessing window geometry
                PostProcessText request;
                request.textAnnotation = t;
                request.opened = false;
                XPDFReader::lookupBool( annotDict, "Open", request.opened );
                ppTextList.append( request );
            }
            else
            {
                // NOTE: please provide testcases for FreeText (don't have any) - Enrico
                // -> textType
                t->setTextType( TextAnnotation::InPlace );
                // -> textFont
                QString textFormat;
                XPDFReader::lookupString( annotDict, "DA", textFormat );
                // TODO, fill t->textFont using textFormat if not empty
                // -> inplaceAlign
                int tmpint = 0;
                XPDFReader::lookupInt( annotDict, "Q", tmpint );
                t->setInplaceAlign( tmpint );
                // -> inplaceText (simple)
                QString tmpstring;
                XPDFReader::lookupString( annotDict, "DS", tmpstring );
                // -> inplaceText (complex override)
                XPDFReader::lookupString( annotDict, "RC", tmpstring );
                t->setInplaceText( tmpstring );
                // -> inplaceCallout
                double c[6];
                int n = XPDFReader::lookupNumArray( annotDict, "CL", c, 6 );
                if ( n >= 4 )
                {
                    QPointF tmppoint;
                    XPDFReader::transform( MTX, c[0], c[1], tmppoint );
                    t->setCalloutPoint( 0, tmppoint );
                    XPDFReader::transform( MTX, c[2], c[3], tmppoint );
                    t->setCalloutPoint( 1, tmppoint );
                    if ( n == 6 )
                    {
                        XPDFReader::transform( MTX, c[4], c[5], tmppoint );
                        t->setCalloutPoint( 2, tmppoint );
                    }
                }
                // -> inplaceIntent
                QString intentName;
                XPDFReader::lookupString( annotDict, "IT", intentName );
                if ( intentName == "FreeTextCallout" )
                    t->setInplaceIntent( TextAnnotation::Callout );
                else if ( intentName == "FreeTextTypeWriter" )
                    t->setInplaceIntent( TextAnnotation::TypeWriter );
            }
        }
        else if ( subType == "Line" || subType == "Polygon" || subType == "PolyLine" )
        {
            // parse LineAnnotation params
            LineAnnotation * l = new LineAnnotation();
            annotation = l;

            // -> linePoints
            double c[100];
            int num = XPDFReader::lookupNumArray( annotDict, (char*)((subType == "Line") ? "L" : "Vertices"), c, 100 );
            if ( num < 4 || (num % 2) != 0 )
            {
                qDebug() << "L/Vertices wrong fol Line/Poly.";
                delete annotation;
                annot.free();
                continue;
            }
            QLinkedList<QPointF> linePoints;
            for ( int i = 0; i < num; i += 2 )
            {
                QPointF p;
                XPDFReader::transform( MTX, c[i], c[i+1], p );
                linePoints.push_back( p );
            }
            l->setLinePoints( linePoints );
            // -> lineStartStyle, lineEndStyle
            Object leArray;
            annotDict->lookup( "LE", &leArray );
            if ( leArray.isArray() && leArray.arrayGetLength() == 2 )
            {
                // -> lineStartStyle
                Object styleObj;
                leArray.arrayGet( 0, &styleObj );
                if ( styleObj.isName() )
                {
                    const char * name = styleObj.getName();
                    if ( !strcmp( name, "Square" ) )
                        l->setLineStartStyle( LineAnnotation::Square );
                    else if ( !strcmp( name, "Circle" ) )
                        l->setLineStartStyle( LineAnnotation::Circle );
                    else if ( !strcmp( name, "Diamond" ) )
                        l->setLineStartStyle( LineAnnotation::Diamond );
                    else if ( !strcmp( name, "OpenArrow" ) )
                        l->setLineStartStyle( LineAnnotation::OpenArrow );
                    else if ( !strcmp( name, "ClosedArrow" ) )
                        l->setLineStartStyle( LineAnnotation::ClosedArrow );
                    else if ( !strcmp( name, "None" ) )
                        l->setLineStartStyle( LineAnnotation::None );
                    else if ( !strcmp( name, "Butt" ) )
                        l->setLineStartStyle( LineAnnotation::Butt );
                    else if ( !strcmp( name, "ROpenArrow" ) )
                        l->setLineStartStyle( LineAnnotation::ROpenArrow );
                    else if ( !strcmp( name, "RClosedArrow" ) )
                        l->setLineStartStyle( LineAnnotation::RClosedArrow );
                    else if ( !strcmp( name, "Slash" ) )
                        l->setLineStartStyle( LineAnnotation::Slash );
                }
                styleObj.free();
                // -> lineEndStyle
                leArray.arrayGet( 1, &styleObj );
                if ( styleObj.isName() )
                {
                    const char * name = styleObj.getName();
                    if ( !strcmp( name, "Square" ) )
                        l->setLineEndStyle( LineAnnotation::Square );
                    else if ( !strcmp( name, "Circle" ) )
                        l->setLineEndStyle( LineAnnotation::Circle );
                    else if ( !strcmp( name, "Diamond" ) )
                        l->setLineEndStyle( LineAnnotation::Diamond );
                    else if ( !strcmp( name, "OpenArrow" ) )
                        l->setLineEndStyle( LineAnnotation::OpenArrow );
                    else if ( !strcmp( name, "ClosedArrow" ) )
                        l->setLineEndStyle( LineAnnotation::ClosedArrow );
                    else if ( !strcmp( name, "None" ) )
                        l->setLineEndStyle( LineAnnotation::None );
                    else if ( !strcmp( name, "Butt" ) )
                        l->setLineEndStyle( LineAnnotation::Butt );
                    else if ( !strcmp( name, "ROpenArrow" ) )
                        l->setLineEndStyle( LineAnnotation::ROpenArrow );
                    else if ( !strcmp( name, "RClosedArrow" ) )
                        l->setLineEndStyle( LineAnnotation::RClosedArrow );
                    else if ( !strcmp( name, "Slash" ) )
                        l->setLineEndStyle( LineAnnotation::Slash );
                }
                styleObj.free();
            }
            leArray.free();
            // -> lineClosed
            l->setLineClosed( subType == "Polygon" );
            // -> lineInnerColor
            QColor tmpcolor = l->lineInnerColor();
            XPDFReader::lookupColor( annotDict, "IC", tmpcolor );
            l->setLineInnerColor( tmpcolor );
            // -> lineLeadingFwdPt
            double tmpdouble = l->lineLeadingForwardPoint();
            XPDFReader::lookupNum( annotDict, "LL", tmpdouble );
            l->setLineLeadingForwardPoint( tmpdouble );
            // -> lineLeadingBackPt
            tmpdouble = l->lineLeadingBackPoint();
            XPDFReader::lookupNum( annotDict, "LLE", tmpdouble );
            l->setLineLeadingBackPoint( tmpdouble );
            // -> lineShowCaption
            bool tmpbool = l->lineShowCaption();
            XPDFReader::lookupBool( annotDict, "Cap", tmpbool );
            l->setLineShowCaption( tmpbool );
            // -> lineIntent
            QString intentName;
            XPDFReader::lookupString( annotDict, "IT", intentName );
            if ( intentName == "LineArrow" )
                l->setLineIntent( LineAnnotation::Arrow );
            else if ( intentName == "LineDimension" )
                l->setLineIntent( LineAnnotation::Dimension );
            else if ( intentName == "PolygonCloud" )
                l->setLineIntent( LineAnnotation::PolygonCloud );
        }
        else if ( subType == "Square" || subType == "Circle" )
        {
            // parse GeomAnnotation params
            GeomAnnotation * g = new GeomAnnotation();
            annotation = g;

            // -> geomType
            if ( subType == "Square" )
                g->setGeomType( GeomAnnotation::InscribedSquare );
            else
                g->setGeomType( GeomAnnotation::InscribedCircle );
            // -> geomInnerColor
            QColor tmpcolor = g->geomInnerColor();
            XPDFReader::lookupColor( annotDict, "IC", tmpcolor );
            g->setGeomInnerColor( tmpcolor );
            // TODO RD
        }
        else if ( subType == "Highlight" || subType == "Underline" ||
                  subType == "Squiggly" || subType == "StrikeOut" )
        {
            // parse HighlightAnnotation params
            HighlightAnnotation * h = new HighlightAnnotation();
            annotation = h;

            // -> highlightType
            if ( subType == "Highlight" )
                h->setHighlightType( HighlightAnnotation::Highlight );
            else if ( subType == "Underline" )
                h->setHighlightType( HighlightAnnotation::Underline );
            else if ( subType == "Squiggly" )
                h->setHighlightType( HighlightAnnotation::Squiggly );
            else if ( subType == "StrikeOut" )
                h->setHighlightType( HighlightAnnotation::StrikeOut );

            // -> highlightQuads
            double c[80];
            int num = XPDFReader::lookupNumArray( annotDict, "QuadPoints", c, 80 );
            if ( num < 8 || (num % 8) != 0 )
            {
                qDebug() << "Wrong QuadPoints for a Highlight annotation.";
                delete annotation;
                annot.free();
                continue;
            }
            QList< HighlightAnnotation::Quad > quads;
            for ( int q = 0; q < num; q += 8 )
            {
                HighlightAnnotation::Quad quad;
                for ( int p = 0; p < 4; p++ )
                    XPDFReader::transform( MTX, c[ q + p*2 ], c[ q + p*2 + 1 ], quad.points[ p ] );
                // ### PDF1.6 specs says that point are in ccw order, but in fact
                // points 3 and 4 are swapped in every PDF around!
                QPointF tmpPoint = quad.points[ 2 ];
                quad.points[ 2 ] = quad.points[ 3 ];
                quad.points[ 3 ] = tmpPoint;
                // initialize other oroperties and append quad
                quad.capStart = true;       // unlinked quads are always capped
                quad.capEnd = true;         // unlinked quads are always capped
                quad.feather = 0.1;         // default feather
                quads.append( quad );
            }
            h->setHighlightQuads( quads );
        }
        else if ( subType == "Stamp" )
        {
            // parse StampAnnotation params
            StampAnnotation * s = new StampAnnotation();
            annotation = s;

            // -> stampIconName
            QString tmpstring = s->stampIconName();
            XPDFReader::lookupName( annotDict, "Name", tmpstring );
            s->setStampIconName( tmpstring );
        }
        else if ( subType == "Ink" )
        {
            // parse InkAnnotation params
            InkAnnotation * k = new InkAnnotation();
            annotation = k;

            // -> inkPaths
            Object pathsArray;
            annotDict->lookup( "InkList", &pathsArray );
            if ( !pathsArray.isArray() || pathsArray.arrayGetLength() < 1 )
            {
                qDebug() << "InkList not present for ink annot";
                delete annotation;
                annot.free();
                continue;
            }
            int pathsNumber = pathsArray.arrayGetLength();
            QList< QLinkedList<QPointF> > inkPaths;
            for ( int m = 0; m < pathsNumber; m++ )
            {
                // transform each path in a list of normalized points ..
                QLinkedList<QPointF> localList;
                Object pointsArray;
                pathsArray.arrayGet( m, &pointsArray );
                if ( pointsArray.isArray() )
                {
                    int pointsNumber = pointsArray.arrayGetLength();
                    for ( int n = 0; n < pointsNumber; n+=2 )
                    {
                        // get the x,y numbers for current point
                        Object numObj;
                        double x = pointsArray.arrayGet( n, &numObj )->getNum();
                        numObj.free();
                        double y = pointsArray.arrayGet( n+1, &numObj )->getNum();
                        numObj.free();
                        // add normalized point to localList
                        QPointF np;
                        XPDFReader::transform( MTX, x, y, np );
                        localList.push_back( np );
                    }
                }
                pointsArray.free();
                // ..and add it to the annotation
                inkPaths.push_back( localList );
            }
            k->setInkPaths( inkPaths );
            pathsArray.free();
        }
        else if ( subType == "Popup" )
        {
            // create PopupWindow and add it to the popupsMap
            PopupWindow * popup = new PopupWindow();
            popupsMap[ annotID ] = popup;
            parseMarkup = false;
            addToPage = false;

            // get window specific properties if any
            popup->shown = false;
            XPDFReader::lookupBool( annotDict, "Open", popup->shown );
            // no need to parse parent annotation id
            //XPDFReader::lookupIntRef( annotDict, "Parent", popup->... );

            // use the 'dummy annotation' for getting other parameters
            popup->dummyAnnotation = new DummyAnnotation();
            annotation = popup->dummyAnnotation;
        }
        else if ( subType == "Link" )
        {
            // parse Link params
            LinkAnnotation * l = new LinkAnnotation();
            annotation = l;

            // -> hlMode
            QString hlModeString;
            XPDFReader::lookupName( annotDict, "H", hlModeString );
            if ( hlModeString == "N" )
                l->setLinkHighlightMode( LinkAnnotation::None );
            else if ( hlModeString == "I" )
                l->setLinkHighlightMode( LinkAnnotation::Invert );
            else if ( hlModeString == "O" )
                l->setLinkHighlightMode( LinkAnnotation::Outline );
            else if ( hlModeString == "P" )
                l->setLinkHighlightMode( LinkAnnotation::Push );

            // -> link region
            double c[8];
            int num = XPDFReader::lookupNumArray( annotDict, "QuadPoints", c, 8 );
            if ( num > 0 && num != 8 )
            {
                qDebug() << "Wrong QuadPoints for a Link annotation.";
                delete annotation;
                annot.free();
                continue;
            }
            if ( num == 8 )
            {
                QPointF tmppoint;
                XPDFReader::transform( MTX, c[ 0 ], c[ 1 ], tmppoint );
                l->setLinkRegionPoint( 0, tmppoint );
                XPDFReader::transform( MTX, c[ 2 ], c[ 3 ], tmppoint );
                l->setLinkRegionPoint( 1, tmppoint );
                XPDFReader::transform( MTX, c[ 4 ], c[ 5 ], tmppoint );
                l->setLinkRegionPoint( 2, tmppoint );
                XPDFReader::transform( MTX, c[ 6 ], c[ 7 ], tmppoint );
                l->setLinkRegionPoint( 3, tmppoint );
            }

            // reading link action
            Object objPA;
            annotDict->lookup( "PA", &objPA );
            if (!objPA.isNull())
            {
                ::LinkAction * a = ::LinkAction::parseAction( &objPA, m_page->parentDoc->doc->getCatalog()->getBaseURI() );
                Link * popplerLink = m_page->convertLinkActionToLink( a, QRectF() );
                if ( popplerLink )
                {
                     l->setLinkDestination( popplerLink );
                }
                objPA.free();
            }
        }
        else
        {
            // MISSING: Caret, FileAttachment, Sound, Movie, Widget,
            //          Screen, PrinterMark, TrapNet, Watermark, 3D
            qDebug() << "Annotation" << subType << "not supported";
            annot.free();
            continue;
        }

        /** 1.3. PARSE common parameters */
        // -> boundary
        double r[4];
        if ( XPDFReader::lookupNumArray( annotDict, "Rect", r, 4 ) != 4 )
        {
            qDebug() << "Rect is missing for annotation.";
            annot.free();
            continue;
        }
        // transform annotation rect to uniform coords
        QPointF topLeft, bottomRight;
        XPDFReader::transform( MTX, r[0], r[1], topLeft );
        XPDFReader::transform( MTX, r[2], r[3], bottomRight );
        QRectF boundaryRect;
        boundaryRect.setTopLeft(topLeft);
        boundaryRect.setBottomRight(bottomRight);
        if ( boundaryRect.left() > boundaryRect.right() )
        {
            double aux = boundaryRect.left();
            boundaryRect.setLeft( boundaryRect.right() );
            boundaryRect.setRight(aux);
        }
        if ( boundaryRect.top() > boundaryRect.bottom() )
        {
            double aux = boundaryRect.top();
            boundaryRect.setTop( boundaryRect.bottom() );
            boundaryRect.setBottom(aux);
           //annotation->rUnscaledWidth = (r[2] > r[0]) ? r[2] - r[0] : r[0] - r[2];
           //annotation->rUnscaledHeight = (r[3] > r[1]) ? r[3] - r[1] : r[1] - r[3];
        }
        annotation->setBoundary( boundaryRect );
        // -> contents
        QString tmpstring = annotation->contents();
        XPDFReader::lookupString( annotDict, "Contents", tmpstring );
        annotation->setContents( tmpstring );
        // -> uniqueName
        tmpstring = annotation->uniqueName();
        XPDFReader::lookupString( annotDict, "NM", tmpstring );
        annotation->setUniqueName( tmpstring );
        // -> modifyDate (and -> creationDate)
        QDateTime tmpdatetime = annotation->modificationDate();
        XPDFReader::lookupDate( annotDict, "M", tmpdatetime );
        annotation->setModificationDate( tmpdatetime );
        if ( annotation->creationDate().isNull() && !annotation->modificationDate().isNull() )
            annotation->setCreationDate( annotation->modificationDate() );
        // -> flags: set the external attribute since it's embedded on file
        int annoflags = 0;
        annoflags |= Annotation::External;
        // -> flags
        int flags = 0;
        XPDFReader::lookupInt( annotDict, "F", flags );
        if ( flags & 0x2 )
            annoflags |= Annotation::Hidden;
        if ( flags & 0x8 )
            annoflags |= Annotation::FixedSize;
        if ( flags & 0x10 )
            annoflags |= Annotation::FixedRotation;
        if ( !(flags & 0x4) )
            annoflags |= Annotation::DenyPrint;
        if ( flags & 0x40 )
            annoflags |= (Annotation::DenyWrite | Annotation::DenyDelete);
        if ( flags & 0x80 )
            annoflags |= Annotation::DenyDelete;
        if ( flags & 0x100 )
            annoflags |= Annotation::ToggleHidingOnMouse;
        annotation->setFlags( annoflags );
        // -> style (Border(old spec), BS, BE)
        double border[3];
        int bn = XPDFReader::lookupNumArray( annotDict, "Border", border, 3 );
        if ( bn == 3 )
        {
            // -> style.xCorners
            annotation->style.xCorners = border[0];
            // -> style.yCorners
            annotation->style.yCorners = border[1];
            // -> style.width
            annotation->style.width = border[2];
        }
        Object bsObj;
        annotDict->lookup( "BS", &bsObj );
        if ( bsObj.isDict() )
        {
            // -> style.width
            XPDFReader::lookupNum( bsObj.getDict(), "W", annotation->style.width );
            // -> style.style
            QString styleName;
            XPDFReader::lookupName( bsObj.getDict(), "S", styleName );
            if ( styleName == "S" )
                annotation->style.style = Annotation::Solid;
            else if ( styleName == "D" )
                annotation->style.style = Annotation::Dashed;
            else if ( styleName == "B" )
                annotation->style.style = Annotation::Beveled;
            else if ( styleName == "I" )
                annotation->style.style = Annotation::Inset;
            else if ( styleName == "U" )
                annotation->style.style = Annotation::Underline;
            // -> style.marks and style.spaces
            Object dashArray;
            bsObj.getDict()->lookup( "D", &dashArray );
            if ( dashArray.isArray() )
            {
                int dashMarks = 3;
                int dashSpaces = 0;
                Object intObj;
                dashArray.arrayGet( 0, &intObj );
                if ( intObj.isInt() )
                    dashMarks = intObj.getInt();
                intObj.free();
                dashArray.arrayGet( 1, &intObj );
                if ( intObj.isInt() )
                    dashSpaces = intObj.getInt();
                intObj.free();
                annotation->style.marks = dashMarks;
                annotation->style.spaces = dashSpaces;
            }
            dashArray.free();
        }
        bsObj.free();
        Object beObj;
        annotDict->lookup( "BE", &beObj );
        if ( beObj.isDict() )
        {
            // -> style.effect
            QString effectName;
            XPDFReader::lookupName( beObj.getDict(), "S", effectName );
            if ( effectName == "C" )
                annotation->style.effect = Annotation::Cloudy;
            // -> style.effectIntensity
            int intensityInt = -1;
            XPDFReader::lookupInt( beObj.getDict(), "I", intensityInt );
            if ( intensityInt != -1 )
                annotation->style.effectIntensity = (double)intensityInt;
        }
        beObj.free();
        // -> style.color
        XPDFReader::lookupColor( annotDict, "C", annotation->style.color );

        /** 1.4. PARSE markup { common, Popup, Revision } parameters */
        if ( parseMarkup )
        {
            // -> creationDate
            tmpdatetime = annotation->creationDate();
            XPDFReader::lookupDate( annotDict, "CreationDate", tmpdatetime );
            annotation->setCreationDate( tmpdatetime );
            // -> style.opacity
            XPDFReader::lookupNum( annotDict, "CA", annotation->style.opacity );
            // -> window.title and author
            XPDFReader::lookupString( annotDict, "T", annotation->window.title );
            annotation->setAuthor( annotation->window.title );
            // -> window.summary
            XPDFReader::lookupString( annotDict, "Subj", annotation->window.summary );
            // -> window.text
            XPDFReader::lookupString( annotDict, "RC", annotation->window.text );

            // if a popup is referenced, schedule for resolving it later
            int popupID = 0;
            XPDFReader::lookupIntRef( annotDict, "Popup", popupID );
            if ( popupID )
            {
                ResolveWindow request;
                request.popupWindowID = popupID;
                request.annotation = annotation;
                resolvePopList.append( request );
            }

            // if an older version is referenced, schedule for reparenting
            int parentID = 0;
            XPDFReader::lookupIntRef( annotDict, "IRT", parentID );
            if ( parentID )
            {
                ResolveRevision request;
                request.nextAnnotation = annotation;
                request.nextAnnotationID = annotID;
                request.prevAnnotationID = parentID;

                // -> request.nextScope
                request.nextScope = Annotation::Reply;
                Object revObj;
                annotDict->lookup( "RT", &revObj );
                if ( revObj.isName() )
                {
                    const char * name = revObj.getName();
                    if ( !strcmp( name, "R" ) )
                        request.nextScope = Annotation::Reply;
                    else if ( !strcmp( name, "Group" ) )
                        request.nextScope = Annotation::Group;
                }
                revObj.free();

                // -> revision.type (StateModel is deduced from type, not parsed)
                request.nextType = Annotation::None;
                annotDict->lookup( "State", &revObj );
                if ( revObj.isString() )
                {
                    const char * name = revObj.getString()->getCString();
                    if ( !strcmp( name, "Marked" ) )
                        request.nextType = Annotation::Marked;
                    else if ( !strcmp( name, "Unmarked" ) )
                        request.nextType = Annotation::Unmarked;
                    else if ( !strcmp( name, "Accepted" ) )
                        request.nextType = Annotation::Accepted;
                    else if ( !strcmp( name, "Rejected" ) )
                        request.nextType = Annotation::Rejected;
                    else if ( !strcmp( name, "Cancelled" ) )
                        request.nextType = Annotation::Cancelled;
                    else if ( !strcmp( name, "Completed" ) )
                        request.nextType = Annotation::Completed;
                    else if ( !strcmp( name, "None" ) )
                        request.nextType = Annotation::None;
                }
                revObj.free();

                // schedule for later reparenting
                resolveRevList.append( request );
            }
        }
        // free annot object
        annot.free();

        /** 1.5. ADD ANNOTATION to the annotationsMap  */
        if ( addToPage )
        {
            if ( annotationsMap.contains( annotID ) )
                qDebug() << "Clash for annotations with ID:" << annotID;
            annotationsMap[ annotID ] = annotation;
        }
    } // end Annotation/PopupWindow parsing loop

    /** 2 - RESOLVE POPUPS (popup.* -> annotation.window) */
    if ( !resolvePopList.isEmpty() && !popupsMap.isEmpty() )
    {
        QLinkedList< ResolveWindow >::iterator it = resolvePopList.begin(),
                                              end = resolvePopList.end();
        for ( ; it != end; ++it )
        {
            const ResolveWindow & request = *it;
            if ( !popupsMap.contains( request.popupWindowID ) )
                // warn aboud problems in popup resolving logic
                qDebug() << "Cannot resolve popup"
                          << request.popupWindowID << ".";
            else
            {
                // set annotation's window properties taking ones from popup
                PopupWindow * pop = popupsMap[ request.popupWindowID ];
                Annotation * pa = pop->dummyAnnotation;
                Annotation::Window & w = request.annotation->window;

                // transfer properties to Annotation's window
                w.flags = pa->flags() & (Annotation::Hidden |
                    Annotation::FixedSize | Annotation::FixedRotation);
                if ( !pop->shown )
                    w.flags |= Annotation::Hidden;
                w.topLeft.setX(pa->boundary().left());
                w.topLeft.setY(pa->boundary().top());
                w.width = (int)( pa->boundary().right() - pa->boundary().left() );
                w.height = (int)( pa->boundary().bottom() - pa->boundary().top() );
            }
        }

        // clear data
        QMap< int, PopupWindow * >::Iterator dIt = popupsMap.begin(), dEnd = popupsMap.end();
        for ( ; dIt != dEnd; ++dIt )
        {
            PopupWindow * p = dIt.value();
            delete p->dummyAnnotation;
            delete p;
        }
    }

    /** 3 - RESOLVE REVISIONS (parent.revisions.append( children )) */
    if ( !resolveRevList.isEmpty() )
    {
        // append children to parents
        QVarLengthArray< int > excludeIDs( resolveRevList.count() );   // can't even reach this size
        int excludeIndex = 0;                       // index in excludeIDs array
        QLinkedList< ResolveRevision >::iterator it = resolveRevList.begin(), end = resolveRevList.end();
        for ( ; it != end; ++it )
        {
            const ResolveRevision & request = *it;
            int parentID = request.prevAnnotationID;
            if ( !annotationsMap.contains( parentID ) )
                // warn about problems in reparenting logic
                qDebug() << "Cannot reparent annotation to"
                          << parentID << ".";
            else
            {
                // compile and add a Revision structure to the parent annotation
                Annotation::Revision childRevision;
                childRevision.annotation = request.nextAnnotation;
                childRevision.scope = request.nextScope;
                childRevision.type = request.nextType;
                annotationsMap[ parentID ]->revisions().append( childRevision );
                // exclude child annotation from being rooted in page
                excludeIDs[ excludeIndex++ ] = request.nextAnnotationID;
            }
        }

        // prevent children from being attached to page as roots
        for ( int i = 0; i < excludeIndex; i++ )
            annotationsMap.remove( excludeIDs[ i ] );
    }

    /** 4 - POSTPROCESS TextAnnotations (when window geom is embedded) */
    if ( !ppTextList.isEmpty() )
    {
        QLinkedList< PostProcessText >::const_iterator it = ppTextList.begin(), end = ppTextList.end();
        for ( ; it != end; ++it )
        {
            const PostProcessText & request = *it;
            Annotation::Window & window = request.textAnnotation->window;
            // if not present, 'create' the window in-place over the annotation
            if ( window.flags == -1 )
            {
                window.flags = 0;
                QRectF geom = request.textAnnotation->boundary();
                // initialize window geometry to annotation's one
                window.width = (int)( geom.right() - geom.left() );
                window.height = (int)( geom.bottom() - geom.top() );
                window.topLeft.setX( geom.left() > 0.0 ? geom.left() : 0.0 );
                window.topLeft.setY( geom.top() > 0.0 ? geom.top() : 0.0 );
            }
            // (pdf) if text is not 'opened', force window hiding. if the window
            // was parsed from popup, the flag should already be set
            if ( !request.opened && window.flags != -1 )
                window.flags |= Annotation::Hidden;
        }
    }

    annotArray.free();
    /** 5 - finally RETURN ANNOTATIONS */
    return annotationsMap.values();
}

QList<FormField*> Page::formFields() const
{
  QList<FormField*> fields;
  ::Page *p = m_page->page;
  ::FormPageWidgets * form = p->getPageWidgets();
  int formcount = form->getNumWidgets();
  for (int i = 0; i < formcount; ++i)
  {
    ::FormWidget *fm = form->getWidget(i);
    FormField * ff = NULL;
    switch (fm->getType())
    {
      case formButton:
      {
        ff = new FormFieldButton(m_page->parentDoc, p, static_cast<FormWidgetButton*>(fm));
      }
      break;

      case formText:
      {
        ff = new FormFieldText(m_page->parentDoc, p, static_cast<FormWidgetText*>(fm));
      }
      break;

      case formChoice:
      {
        ff = new FormFieldChoice(m_page->parentDoc, p, static_cast<FormWidgetChoice*>(fm));
      }
      break;

      default: ;
    }

    if (ff)
      fields.append(ff);
  }

  return fields;
}

double Page::duration() const
{
  return m_page->page->getDuration();
}

QString Page::label() const
{
  GooString goo;
  if (!m_page->parentDoc->doc->getCatalog()->indexToLabel(m_page->index, &goo))
    return QString();

  return QString(goo.getCString());
}


}
