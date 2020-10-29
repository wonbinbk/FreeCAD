/***************************************************************************
 *   Copyright (c) 2002 Jürgen Riegel <juergen.riegel@web.de>              *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/


#include "PreCompiled.h"

#ifndef _PreComp_
# include <sstream>
# include <BRepAdaptor_Curve.hxx>
# include <BRepAdaptor_Surface.hxx>
# include <BRepBndLib.hxx>
# include <BRepBuilderAPI_GTransform.hxx>
# include <Bnd_Box.hxx>
# include <BRepTools.hxx>
# include <BRepTools_ShapeSet.hxx>
# include <BRepBuilderAPI_Copy.hxx>
# include <TopTools_HSequenceOfShape.hxx>
# include <TopTools_MapOfShape.hxx>
# include <TopoDS.hxx>
# include <TopoDS_Iterator.hxx>
# include <TopExp.hxx>
# include <Standard_Failure.hxx>
# include <Standard_Version.hxx>
# include <gp_GTrsf.hxx>
# include <gp_Trsf.hxx>
# include <BRepBuilderAPI_MakeShape.hxx>
# include <TopTools_ListIteratorOfListOfShape.hxx>

#if OCC_VERSION_HEX >= 0x060800
# include <OSD_OpenFile.hxx>
#endif

#endif // _PreComp_

#include <Base/Console.h>
#include <Base/Writer.h>
#include <Base/Reader.h>
#include <Base/Exception.h>
#include <Base/FileInfo.h>
#include <Base/Stream.h>
#include <App/Application.h>
#include <App/Document.h>
#include <App/DocumentObject.h>
#include <App/ObjectIdentifier.h>
#include <App/GeoFeature.h>

#include "PartPyCXX.h"
#include "PropertyTopoShape.h"
#include "TopoShapePy.h"
#include "TopoShapeFacePy.h"
#include "TopoShapeEdgePy.h"
#include "TopoShapeWirePy.h"
#include "TopoShapeVertexPy.h"
#include "TopoShapeSolidPy.h"
#include "TopoShapeShellPy.h"
#include "TopoShapeCompSolidPy.h"
#include "TopoShapeCompoundPy.h"
#include "PartParams.h"

FC_LOG_LEVEL_INIT("PropShape",true,true);

using namespace Part;

TYPESYSTEM_SOURCE(Part::PropertyPartShape , App::PropertyComplexGeoData)

PropertyPartShape::PropertyPartShape()
{
}

PropertyPartShape::~PropertyPartShape()
{
}

void PropertyPartShape::setValue(const TopoShape& sh)
{
    aboutToSetValue();
    _Shape = sh;
    auto obj = dynamic_cast<App::DocumentObject*>(getContainer());
    if(obj) {
        auto tag = obj->getID();
        if(_Shape.Tag && tag!=_Shape.Tag) {
            auto hasher = _Shape.Hasher?_Shape.Hasher:obj->getDocument()->getStringHasher();
            _Shape.reTagElementMap(tag,hasher);
        } else
            _Shape.Tag = obj->getID();
    }
    hasSetValue();
    _Ver.clear();
}

void PropertyPartShape::setValue(const TopoDS_Shape& sh, bool resetElementMap)
{
    aboutToSetValue();
    auto obj = dynamic_cast<App::DocumentObject*>(getContainer());
    if(obj)
        _Shape.Tag = obj->getID();
    _Shape.setShape(sh,resetElementMap);
    hasSetValue();
    _Ver.clear();
}

const TopoDS_Shape& PropertyPartShape::getValue(void)const
{
    return _Shape.getShape();
}

TopoShape PropertyPartShape::getShape() const
{
    _Shape.initCache(-1);
    auto res = _Shape;
    if(!res.Tag) {
        auto parent = Base::freecad_dynamic_cast<App::DocumentObject>(getContainer());
        if(parent)
            res.Tag = parent->getID();
    }
    return res;
}

const Data::ComplexGeoData* PropertyPartShape::getComplexData() const
{
    _Shape.initCache(-1);
    return &(this->_Shape);
}

Base::BoundBox3d PropertyPartShape::getBoundingBox() const
{
    Base::BoundBox3d box;
    if (_Shape.getShape().IsNull())
        return box;
    try {
        // If the shape is empty an exception may be thrown
        Bnd_Box bounds;
        BRepBndLib::Add(_Shape.getShape(), bounds);
        bounds.SetGap(0.0);
        Standard_Real xMin, yMin, zMin, xMax, yMax, zMax;
        bounds.Get(xMin, yMin, zMin, xMax, yMax, zMax);

        box.MinX = xMin;
        box.MaxX = xMax;
        box.MinY = yMin;
        box.MaxY = yMax;
        box.MinZ = zMin;
        box.MaxZ = zMax;
    }
    catch (Standard_Failure&) {
    }

    return box;
}

void PropertyPartShape::transformGeometry(const Base::Matrix4D &rclTrf)
{
    aboutToSetValue();
    _Shape.transformGeometry(rclTrf);
    hasSetValue();
}

PyObject *PropertyPartShape::getPyObject(void)
{
    auto prop = static_cast<Base::PyObjectBase*>(Py::new_reference_to(shape2pyshape(getShape())));
    if (prop) prop->setConst();
    return prop;
}

void PropertyPartShape::setPyObject(PyObject *value)
{
    if (PyObject_TypeCheck(value, &(TopoShapePy::Type))) {
        auto shape = *static_cast<TopoShapePy*>(value)->getTopoShapePtr();
        auto owner = dynamic_cast<App::DocumentObject*>(getContainer());
        if(owner && owner->getDocument()) {
            if(shape.Tag || shape.getElementMapSize(false)
                         || shape.hasPendingElementMap())
            {
                // We can't trust the meaning of the input shape tag, so we
                // remap anyway
                TopoShape res(owner->getID(),owner->getDocument()->getStringHasher(),shape.getShape());
                res.delayMapSubElement(shape);
                shape = res;
            }else{
                shape.Tag = owner->getID();
                shape.Hasher.reset();
            }
        }
        setValue(shape);
    }
    else {
        std::string error = std::string("type must be 'Shape', not ");
        error += value->ob_type->tp_name;
        throw Base::TypeError(error);
    }
}

App::Property *PropertyPartShape::Copy(void) const
{
    PropertyPartShape *prop = new PropertyPartShape();

    if (PartParams::ShapePropertyCopy()) {
        // makECopy() consume too much memory for complex geometry.
        prop->_Shape = this->_Shape.makECopy();
    } else
        prop->_Shape = this->_Shape;
    prop->_Ver = this->_Ver;
    return prop;
}

void PropertyPartShape::Paste(const App::Property &from)
{
    auto prop = Base::freecad_dynamic_cast<const PropertyPartShape>(&from);
    if(prop) {
        setValue(prop->_Shape);
        _Ver = prop->_Ver;
    }
}

unsigned int PropertyPartShape::getMemSize (void) const
{
    return _Shape.getMemSize();
}

void PropertyPartShape::getPaths(std::vector<App::ObjectIdentifier> &paths) const
{
    // The paths below seem to only there for expression completer. They are no
    // longer required because the completer will now dig into all Python attributes.
    (void)paths;

    // paths.push_back(App::ObjectIdentifier(getContainer()) << App::ObjectIdentifier::Component::SimpleComponent(getName())
    //                 << App::ObjectIdentifier::Component::SimpleComponent(App::ObjectIdentifier::String("ShapeType")));
    // paths.push_back(App::ObjectIdentifier(getContainer()) << App::ObjectIdentifier::Component::SimpleComponent(getName())
    //                 << App::ObjectIdentifier::Component::SimpleComponent(App::ObjectIdentifier::String("Orientation")));
    // paths.push_back(App::ObjectIdentifier(getContainer()) << App::ObjectIdentifier::Component::SimpleComponent(getName())
    //                 << App::ObjectIdentifier::Component::SimpleComponent(App::ObjectIdentifier::String("Length")));
    // paths.push_back(App::ObjectIdentifier(getContainer()) << App::ObjectIdentifier::Component::SimpleComponent(getName())
    //                 << App::ObjectIdentifier::Component::SimpleComponent(App::ObjectIdentifier::String("Area")));
    // paths.push_back(App::ObjectIdentifier(getContainer()) << App::ObjectIdentifier::Component::SimpleComponent(getName())
    //                 << App::ObjectIdentifier::Component::SimpleComponent(App::ObjectIdentifier::String("Volume")));
}

static void BRepTools_Write(const TopoDS_Shape& Sh, Standard_OStream& S);

void PropertyPartShape::Save (Base::Writer &writer) const
{
    //See SaveDocFile(), RestoreDocFile()
    writer.Stream() << writer.ind() << "<Part";
    bool saveHasher=false;
    auto owner = dynamic_cast<App::DocumentObject*>(getContainer());
    if(owner && !_Shape.Hasher.isNull()) {
        auto ret = owner->getDocument()->addStringHasher(_Shape.Hasher);
        writer.Stream() << " HasherIndex=\"" << ret.second << '"';
        if(ret.first) {
            saveHasher = true;
            writer.Stream() << " SaveHasher=\"1\"";
        }
    }
    std::string version;
    // If exporting, do not export mapped element name, but still make a mark
    if(owner) {
        if(!owner->isExporting())
            version = _Ver.size()?_Ver:owner->getElementMapVersion(this);
    }else
        version = _Ver.size()?_Ver:_Shape.getElementMapVersion();
    writer.Stream() << " ElementMap=\"" << version << '"';

    bool binary = writer.getMode("BinaryBrep");
    bool toXML = writer.getFileVersion()>1 && writer.isForceXML()>=(binary?3:2);
    if(!toXML) {
        writer.Stream() << " file=\""
            << writer.addFile(getFileName(binary?".bin":".brp"), this)
            << "\"/>\n";
    } else if(binary) {
        writer.Stream() << " binary=\"1\">\n";
        TopoShape shape;
        shape.setShape(_Shape.getShape());
        shape.exportBinary(writer.beginCharStream(true));
        writer.endCharStream() <<  writer.ind() << "</Part>\n";
    } else {
        writer.Stream() << " brep=\"1\">\n";
        BRepTools_Write(_Shape.getShape(), writer.beginCharStream(false)<<'\n');
        writer.endCharStream() << '\n' << writer.ind() << "</Part>\n";
    }

    if(saveHasher) {
        if(!toXML && writer.getFileVersion()>1)
            _Shape.Hasher->setPersistenceFileName(getFileName(".Table").c_str());
        else
            _Shape.Hasher->setPersistenceFileName(0);
        _Shape.Hasher->Save(writer);
    }
    if(version.size()) {
        if(!toXML && writer.getFileVersion()>1)
            _Shape.setPersistenceFileName(getFileName(".Map").c_str());
        else
            _Shape.setPersistenceFileName(0);
        _Shape.Save(writer);
    }
}

std::string PropertyPartShape::getElementMapVersion(bool restored) const {
    if(restored)
        return _Ver;
    return PropertyComplexGeoData::getElementMapVersion(false);
}

void PropertyPartShape::Restore(Base::XMLReader &reader)
{
    reader.readElement("Part");

    auto owner = Base::freecad_dynamic_cast<App::DocumentObject>(getContainer());
    _Ver.clear();
    bool has_ver = reader.hasAttribute("ElementMap");
    if(has_ver)
        _Ver = reader.getAttribute("ElementMap");

    int hasher_idx = reader.getAttributeAsInteger("HasherIndex","-1");
    int save_hasher = reader.getAttributeAsInteger("SaveHasher","");

    TopoDS_Shape sh;

    if(reader.hasAttribute("file")) {
        std::string file = reader.getAttribute("file");
        if (!file.empty()) {
            // initiate a file read
            reader.addFile(file.c_str(),this);
        }
    } else if(reader.getAttributeAsInteger("binary","")) {
        TopoShape shape;
        shape.importBinary(reader.beginCharStream(true));
        sh = shape.getShape();
    } else if(reader.getAttributeAsInteger("brep","")) {
        BRep_Builder builder;
        BRepTools::Read(sh, reader.beginCharStream(false), builder);
    }

    reader.readEndElement("Part");

    if(owner && hasher_idx>=0) {
        _Shape.Hasher = owner->getDocument()->getStringHasher(hasher_idx);
        if(save_hasher)
            _Shape.Hasher->Restore(reader);
    }

    if(has_ver) {
        if(owner && owner->getDocument()->testStatus(App::Document::PartialDoc))
            _Shape.Restore(reader);
        else if(_Ver.empty()) {
            // empty string marks the need for recompute after import
            if(owner) owner->getDocument()->addRecomputeObject(owner);
        }else{
            _Shape.Restore(reader);
            auto ver = owner?owner->getElementMapVersion(this):_Shape.getElementMapVersion();
            if(ver!=_Ver) {
                if(!owner || !owner->getNameInDocument() || !_Shape.getElementMapSize())
                    _Ver = ver;
                else {
                    // version mismatch, signal for regenerating.
                    static const char *warnedDoc=0;
                    if(warnedDoc != owner->getDocument()->getName()) {
                        warnedDoc = owner->getDocument()->getName();
                        FC_WARN("Recomputation required for document '" << warnedDoc 
                                << "' on geo element version change in " << getFullName()
                                << ": " << _Ver << " -> " << ver);
                    }
                    owner->getDocument()->addRecomputeObject(owner);
                }
            }
        }
    } else if(owner && !owner->getDocument()->testStatus(App::Document::PartialDoc)) {
        static int buildElementMap = -1;
        if(buildElementMap<0) {
            static ParameterGrp::handle hGrp;
            if (!hGrp)
               hGrp = App::GetApplication().GetParameterGroupByPath(
                    "User parameter:BaseApp/Preferences/Mod/Part/General");
            buildElementMap = hGrp->GetBool("AutoElementMap",true)?1:0;
        }
        if(buildElementMap) {
            FC_WARN("Pending recompute for generating element map: " << owner->getFullName());
            owner->getDocument()->addRecomputeObject(owner);
        }
    }

    aboutToSetValue();
    _Shape.setShape(sh,false);
    hasSetValue();
}

// The following two functions are copied from OCCT BRepTools.cxx and modified
// to disable saving of triangulation
//
static void BRepTools_Write(const TopoDS_Shape& Sh, Standard_OStream& S) {
  BRepTools_ShapeSet SS(Standard_False);
  // SS.SetProgress(PR);
  SS.Add(Sh);
  SS.Write(S);
  SS.Write(Sh,S);
}

static Standard_Boolean  BRepTools_Write(const TopoDS_Shape& Sh, const Standard_CString File)
{
  std::ofstream os;
#if OCC_VERSION_HEX >= 0x060800
  OSD_OpenStream(os, File, std::ios::out);
#else
  os.open(File, std::ios::out);
#endif
  if (!os.rdbuf()->is_open()) return Standard_False;

  Standard_Boolean isGood = (os.good() && !os.eof());
  if(!isGood)
    return isGood;

  BRepTools_ShapeSet SS(Standard_False);
  // SS.SetProgress(PR);
  SS.Add(Sh);

  os << "DBRep_DrawableShape\n";  // for easy Draw read
  SS.Write(os);
  isGood = os.good();
  if(isGood )
    SS.Write(Sh,os);
  os.flush();
  isGood = os.good();

  errno = 0;
  os.close();
  isGood = os.good() && isGood && !errno;

  return isGood;
}

void PropertyPartShape::SaveDocFile (Base::Writer &writer) const
{
    // Even if the shape is null, we shall still save it, so that there is
    // some content inside the file, or else, we'll get some annoying error
    // message when restoring.
    //
    // if (_Shape.getShape().IsNull())
    //     return;

    TopoDS_Shape myShape = _Shape.getShape();
    if(writer.getMode("BinaryBrep")) {
        TopoShape shape;
        shape.setShape(myShape);
        shape.exportBinary(writer.Stream());
    }
    else {
        static 
        bool direct = App::GetApplication().GetParameterGroupByPath
            ("User parameter:BaseApp/Preferences/Mod/Part/General")->GetBool("DirectAccess", true);
        if (!direct) {
            // create a temporary file and copy the content to the zip stream
            // once the tmp. filename is known use always the same because otherwise
            // we may run into some problems on the Linux platform
            static Base::FileInfo fi(App::Application::getTempFileName());

            if (!BRepTools_Write(myShape,(Standard_CString)fi.filePath().c_str())) {
                // Note: Do NOT throw an exception here because if the tmp. file could
                // not be created we should not abort.
                // We only print an error message but continue writing the next files to the
                // stream...
                App::PropertyContainer* father = this->getContainer();
                if (father && father->isDerivedFrom(App::DocumentObject::getClassTypeId())) {
                    App::DocumentObject* obj = static_cast<App::DocumentObject*>(father);
                    Base::Console().Error("Shape of '%s' cannot be written to BRep file '%s'\n",
                        obj->Label.getValue(),fi.filePath().c_str());
                }
                else {
                    Base::Console().Error("Cannot save BRep file '%s'\n", fi.filePath().c_str());
                }

                std::stringstream ss;
                ss << "Cannot save BRep file '" << fi.filePath() << "'";
                writer.addError(ss.str());
            }

            Base::ifstream file(fi, std::ios::in | std::ios::binary);
            if (file) {
                //unsigned long ulSize = 0;
                std::streambuf* buf = file.rdbuf();
                //if (buf) {
                //    unsigned long ulCurr;
                //    ulCurr = buf->pubseekoff(0, std::ios::cur, std::ios::in);
                //    ulSize = buf->pubseekoff(0, std::ios::end, std::ios::in);
                //    buf->pubseekoff(ulCurr, std::ios::beg, std::ios::in);
                //}

                // read in the ASCII file and write back to the stream
                //std::strstreambuf sbuf(ulSize);
                //file >> &sbuf;
                //writer.Stream() << &sbuf;
                writer.Stream() << buf;
            }

            file.close();
            // remove temp file
            fi.deleteFile();
        }
        else {
            BRepTools_Write(myShape, writer.Stream());
        }
    }
}

void PropertyPartShape::RestoreDocFile(Base::Reader &reader)
{
    // save the element map
    auto elementMap = _Shape.resetElementMap();
    auto hasher = _Shape.Hasher;

    Base::FileInfo brep(reader.getFileName());
    TopoShape shape;
    if (brep.hasExtension("bin")) {
        shape.importBinary(reader);
    }
    else {
        TopoDS_Shape sh;
        static ParameterGrp::handle hGrp;
        if (!hGrp)
            hGrp = App::GetApplication().GetParameterGroupByPath(
                "User parameter:BaseApp/Preferences/Mod/Part/General");
        bool direct = hGrp->GetBool("DirectAccess", true);
        if (!direct) {
            BRep_Builder builder;
            // create a temporary file and copy the content from the zip stream
            Base::FileInfo fi(App::Application::getTempFileName());

            // read in the ASCII file and write back to the file stream
            Base::ofstream file(fi, std::ios::out | std::ios::binary);
            unsigned long ulSize = 0;
            if (reader) {
                std::streambuf* buf = file.rdbuf();
                reader >> buf;
                file.flush();
                ulSize = buf->pubseekoff(0, std::ios::cur, std::ios::in);
            }
            file.close();

            // Read the shape from the temp file, if the file is empty the stored shape was already empty.
            // If it's still empty after reading the (non-empty) file there must occurred an error.
            if (ulSize > 0) {
                if (!BRepTools::Read(sh, (Standard_CString)fi.filePath().c_str(), builder)) {
                    // Note: Do NOT throw an exception here because if the tmp. created file could
                    // not be read it's NOT an indication for an invalid input stream 'reader'.
                    // We only print an error message but continue reading the next files from the
                    // stream...
                    App::PropertyContainer* father = this->getContainer();
                    if (father && father->isDerivedFrom(App::DocumentObject::getClassTypeId())) {
                        App::DocumentObject* obj = static_cast<App::DocumentObject*>(father);
                        Base::Console().Error("BRep file '%s' with shape of '%s' seems to be empty\n",
                            fi.filePath().c_str(),obj->Label.getValue());
                    }
                    else {
                        Base::Console().Warning("Loaded BRep file '%s' seems to be empty\n", fi.filePath().c_str());
                    }
                }
            }

            // delete the temp file
            fi.deleteFile();
            shape.setShape(sh);
        }
        else {
            BRep_Builder builder;
            BRepTools::Read(sh, reader, builder);
            shape.setShape(sh);
        }
    }

    std::string ver = _Ver;
    // restore the element map
    shape.Hasher = hasher;
    shape.resetElementMap(elementMap);
    setValue(shape);
    _Ver = ver;
}

// -------------------------------------------------------------------------

ShapeHistory::ShapeHistory(BRepBuilderAPI_MakeShape& mkShape, TopAbs_ShapeEnum type,
                           const TopoDS_Shape& newS, const TopoDS_Shape& oldS)
{
    reset(mkShape,type,newS,oldS);
}

void ShapeHistory::reset(BRepBuilderAPI_MakeShape& mkShape, TopAbs_ShapeEnum type,
                                 const TopoDS_Shape& newS, const TopoDS_Shape& oldS)
{
    shapeMap.clear();
    this->type = type;

    TopTools_IndexedMapOfShape newM, oldM;
    TopExp::MapShapes(newS, type, newM); // map containing all old objects of type "type"
    TopExp::MapShapes(oldS, type, oldM); // map containing all new objects of type "type"

    // Look at all objects in the old shape and try to find the modified object in the new shape
    for (int i=1; i<=oldM.Extent(); i++) {
        bool found = false;
        TopTools_ListIteratorOfListOfShape it;
        // Find all new objects that are a modification of the old object (e.g. a face was resized)
        for (it.Initialize(mkShape.Modified(oldM(i))); it.More(); it.Next()) {
            found = true;
            for (int j=1; j<=newM.Extent(); j++) { // one old object might create several new ones!
                if (newM(j).IsPartner(it.Value())) {
                    shapeMap[i-1].push_back(j-1); // adjust indices to start at zero
                    break;
                }
            }
        }

        // Find all new objects that were generated from an old object (e.g. a face generated from an edge)
        for (it.Initialize(mkShape.Generated(oldM(i))); it.More(); it.Next()) {
            found = true;
            for (int j=1; j<=newM.Extent(); j++) {
                if (newM(j).IsPartner(it.Value())) {
                    shapeMap[i-1].push_back(j-1);
                    break;
                }
            }
        }

        if (!found) {
            // Find all old objects that don't exist any more (e.g. a face was completely cut away)
            if (mkShape.IsDeleted(oldM(i))) {
                shapeMap[i-1] = std::vector<int>();
            }
            else {
                // Mop up the rest (will this ever be reached?)
                for (int j=1; j<=newM.Extent(); j++) {
                    if (newM(j).IsPartner(oldM(i))) {
                        shapeMap[i-1].push_back(j-1);
                        break;
                    }
                }
            }
        }
    }
}

void ShapeHistory::join(const ShapeHistory& newH)
{
    ShapeHistory join;

    for (ShapeHistory::MapList::const_iterator it = shapeMap.begin(); it != shapeMap.end(); ++it) {
        int old_shape_index = it->first;
        if (it->second.empty())
            join.shapeMap[old_shape_index] = ShapeHistory::List();
        for (ShapeHistory::List::const_iterator jt = it->second.begin(); jt != it->second.end(); ++jt) {
            ShapeHistory::MapList::const_iterator kt = newH.shapeMap.find(*jt);
            if (kt != newH.shapeMap.end()) {
                ShapeHistory::List& ary = join.shapeMap[old_shape_index];
                ary.insert(ary.end(), kt->second.begin(), kt->second.end());
            }
        }
    }

    shapeMap.swap(join.shapeMap);
}

// -------------------------------------------------------------------------

TYPESYSTEM_SOURCE(Part::PropertyShapeHistory , App::PropertyLists)

PropertyShapeHistory::PropertyShapeHistory()
{
}

PropertyShapeHistory::~PropertyShapeHistory()
{
}

void PropertyShapeHistory::setValue(const ShapeHistory& sh)
{
    aboutToSetValue();
    _lValueList.resize(1);
    _lValueList[0] = sh;
    hasSetValue();
}

void PropertyShapeHistory::setValues(const std::vector<ShapeHistory>& values)
{
    aboutToSetValue();
    _lValueList = values;
    hasSetValue();
}

PyObject *PropertyShapeHistory::getPyObject(void)
{
    return Py::new_reference_to(Py::None());
}

void PropertyShapeHistory::setPyObject(PyObject *)
{
}

void PropertyShapeHistory::Save (Base::Writer &) const
{
}

void PropertyShapeHistory::Restore(Base::XMLReader &)
{
}

void PropertyShapeHistory::SaveDocFile (Base::Writer &) const
{
}

void PropertyShapeHistory::RestoreDocFile(Base::Reader &)
{
}

App::Property *PropertyShapeHistory::Copy(void) const
{
    PropertyShapeHistory *p= new PropertyShapeHistory();
    p->_lValueList = _lValueList;
    return p;
}

void PropertyShapeHistory::Paste(const Property &from)
{
    aboutToSetValue();
    _lValueList = dynamic_cast<const PropertyShapeHistory&>(from)._lValueList;
    hasSetValue();
}

// -------------------------------------------------------------------------

TYPESYSTEM_SOURCE(Part::PropertyFilletEdges , App::PropertyLists)

PropertyFilletEdges::PropertyFilletEdges()
{
}

PropertyFilletEdges::~PropertyFilletEdges()
{
}

void PropertyFilletEdges::setValue(int id, double r1, double r2)
{
    setValue(FilletElement(id,r1,r2));
}

PyObject *PropertyFilletEdges::getPyObject(void)
{
    Py::List list(getSize());
    std::vector<FilletElement>::const_iterator it;
    int index = 0;
    for (it = _lValueList.begin(); it != _lValueList.end(); ++it) {
        Py::Tuple ent(3);
#if PY_MAJOR_VERSION >= 3
        ent.setItem(0, Py::Long(it->edgeid));
#else
        ent.setItem(0, Py::Int(it->edgeid));
#endif
        ent.setItem(1, Py::Float(it->radius1));
        ent.setItem(2, Py::Float(it->radius2));
        list[index++] = ent;
    }

    return Py::new_reference_to(list);
}

FilletElement PropertyFilletEdges::getPyValue(PyObject *item) const
{
    FilletElement fe;
    if(!PyObject_TypeCheck(item, &PyTuple_Type))
        throw Base::TypeError();

    try {
        Py::Tuple ent(item);
#if PY_MAJOR_VERSION >= 3
        fe.edgeid = (int)Py::Long(ent.getItem(0));
#else
        fe.edgeid = (int)Py::Int(ent.getItem(0));
#endif
        fe.radius1 = (double)Py::Float(ent.getItem(1));
        fe.radius2 = (double)Py::Float(ent.getItem(2));
    } catch (Py::Exception &) {
        Base::PyException::ThrowException();
    }
    return fe;
}

bool PropertyFilletEdges::saveXML(Base::Writer &writer) const {
    writer.Stream() << ">\n";
    for(auto &v : _lValueList)
        writer.Stream() << v.edgeid << ' ' << v.radius1 << ' ' << v.radius2 << '\n';
    return false;
}

void PropertyFilletEdges::restoreXML(Base::XMLReader &reader)
{
    unsigned count = reader.getAttributeAsUnsigned("count");
    auto &s = reader.beginCharStream(false);
    std::vector<FilletElement> values(count);
    for(auto &v : values) 
        s >> v.edgeid >> v.radius1 >> v.radius2;
    reader.endCharStream();
    setValues(std::move(values));
}

void PropertyFilletEdges::saveStream(Base::OutputStream &str) const
{
    for (std::vector<FilletElement>::const_iterator it = _lValueList.begin(); it != _lValueList.end(); ++it) {
        str << it->edgeid << it->radius1 << it->radius2;
    }
}

void PropertyFilletEdges::restoreStream(Base::InputStream &str, unsigned uCt)
{
    std::vector<FilletElement> values(uCt);
    for (std::vector<FilletElement>::iterator it = values.begin(); it != values.end(); ++it) {
        str >> it->edgeid >> it->radius1 >> it->radius2;
    }
    setValues(std::move(values));
}

App::Property *PropertyFilletEdges::Copy(void) const
{
    PropertyFilletEdges *p= new PropertyFilletEdges();
    p->_lValueList = _lValueList;
    return p;
}

void PropertyFilletEdges::Paste(const Property &from)
{
    setValues(dynamic_cast<const PropertyFilletEdges&>(from)._lValueList);
}

// -------------------------------------------------------------------------

TYPESYSTEM_SOURCE(Part::PropertyShapeCache, App::Property);

App::Property *PropertyShapeCache::Copy(void) const {
    return new PropertyShapeCache();
}

void PropertyShapeCache::Paste(const App::Property &) {
    cache.clear();
}

void PropertyShapeCache::Save (Base::Writer &) const
{
}

void PropertyShapeCache::Restore(Base::XMLReader &)
{
}

PyObject *PropertyShapeCache::getPyObject() {
    Py::List res;
    for(auto &v : cache)
        res.append(Py::TupleN(Py::String(v.first),shape2pyshape(v.second)));
    return Py::new_reference_to(res);
}

void PropertyShapeCache::setPyObject(PyObject *value) {
    if(!value)
        return;
    if(value == Py_None)
        cache.clear();
    App::PropertyStringList prop;
    prop.setPyObject(value);
    for(const auto &sub : prop.getValues())
        cache.erase(sub);
}

#define SHAPE_CACHE_NAME "_Part_ShapeCache"
PropertyShapeCache *PropertyShapeCache::get(const App::DocumentObject *obj, bool create) {
    auto prop = Base::freecad_dynamic_cast<PropertyShapeCache>(
            obj->getDynamicPropertyByName(SHAPE_CACHE_NAME));
    if(prop && prop->getContainer()==obj)
        return prop;
    if(!create)
        return 0;

    prop = static_cast<PropertyShapeCache*>(
            const_cast<App::DocumentObject*>(obj)->addDynamicProperty("Part::PropertyShapeCache",
                SHAPE_CACHE_NAME,"Part","Shape cache",
                App::Prop_NoPersist|App::Prop_Output|App::Prop_Hidden));
    if(!prop) 
        FC_ERR("Failed to add shape cache for " << obj->getFullName());
    else
        prop->connChanged = const_cast<App::DocumentObject*>(obj)->signalEarlyChanged.connect(
                boost::bind(&PropertyShapeCache::slotChanged,prop,_1,_2));
    return prop;
}

bool PropertyShapeCache::getShape(const App::DocumentObject *obj, TopoShape &shape, const char *subname) {
    if (PartParams::DisableShapeCache())
        return false;
    auto prop = get(obj,false);
    if(!prop)
        return false;
    if(!subname) subname = "";
    auto it = prop->cache.find(subname);
    if(it!=prop->cache.end()) {
        shape = it->second;
        return !shape.isNull();
    }
    return false;
}

void PropertyShapeCache::setShape(
        const App::DocumentObject *obj, const TopoShape &shape, const char *subname) 
{
    if (PartParams::DisableShapeCache())
        return;
    auto prop = get(obj,true);
    if(!prop)
        return;
    if(!subname) subname = "";
    prop->cache[subname] = shape;
}

void PropertyShapeCache::slotChanged(const App::DocumentObject &, const App::Property &prop) {
    auto propName = prop.getName();
    if(!propName) return;
    if(strcmp(propName,"Group")==0 || 
            strcmp(propName,"Shape")==0 ||
            strstr(propName,"Touched")!=0)
    {
        FC_LOG("clear shape cache on changed " << prop.getFullName());
        cache.clear();
    }
}

