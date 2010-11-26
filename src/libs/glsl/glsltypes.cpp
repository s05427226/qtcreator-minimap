/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2010 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** Commercial Usage
**
** Licensees holding valid Qt Commercial licenses may use this file in
** accordance with the Qt Commercial License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Nokia.
**
** GNU Lesser General Public License Usage
**
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** If you are unsure which license is appropriate for your use, please
** contact the sales department at http://qt.nokia.com/contact.
**
**************************************************************************/

#include "glsltypes.h"
#include "glslsymbols.h"
#include "glslengine.h"

using namespace GLSL;

bool UndefinedType::isEqualTo(const Type *other) const
{
    if (other && other->asUndefinedType() != 0)
        return true;
    return false;
}

bool UndefinedType::isLessThan(const Type *other) const
{
    Q_UNUSED(other);
    Q_ASSERT(other != 0);
    Q_ASSERT(other->asUndefinedType() != 0);
    return false;
}

bool VoidType::isEqualTo(const Type *other) const
{
    if (other && other->asVoidType() != 0)
        return true;
    return false;
}

bool VoidType::isLessThan(const Type *other) const
{
    Q_UNUSED(other);
    Q_ASSERT(other != 0);
    Q_ASSERT(other->asVoidType() != 0);
    return false;
}

bool BoolType::isEqualTo(const Type *other) const
{
    if (other && other->asBoolType() != 0)
        return true;
    return false;
}

bool BoolType::isLessThan(const Type *other) const
{
    Q_UNUSED(other);
    Q_ASSERT(other != 0);
    Q_ASSERT(other->asBoolType() != 0);
    return false;
}

bool IntType::isEqualTo(const Type *other) const
{
    if (other && other->asIntType() != 0)
        return true;
    return false;
}

bool IntType::isLessThan(const Type *other) const
{
    Q_UNUSED(other);
    Q_ASSERT(other != 0);
    Q_ASSERT(other->asIntType() != 0);
    return false;
}

bool UIntType::isEqualTo(const Type *other) const
{
    if (other && other->asUIntType() != 0)
        return true;
    return false;
}

bool UIntType::isLessThan(const Type *other) const
{
    Q_UNUSED(other);
    Q_ASSERT(other != 0);
    Q_ASSERT(other->asUIntType() != 0);
    return false;
}

bool FloatType::isEqualTo(const Type *other) const
{
    if (other && other->asFloatType() != 0)
        return true;
    return false;
}

bool FloatType::isLessThan(const Type *other) const
{
    Q_UNUSED(other);
    Q_ASSERT(other != 0);
    Q_ASSERT(other->asFloatType() != 0);
    return false;
}

bool DoubleType::isEqualTo(const Type *other) const
{
    if (other && other->asDoubleType() != 0)
        return true;
    return false;
}

bool DoubleType::isLessThan(const Type *other) const
{
    Q_UNUSED(other);
    Q_ASSERT(other != 0);
    Q_ASSERT(other->asDoubleType() != 0);
    return false;
}

void VectorType::add(Symbol *symbol)
{
    _members.insert(symbol->name(), symbol);
}

Symbol *VectorType::find(const QString &name) const
{
    return _members.value(name);
}

void VectorType::populateMembers(Engine *engine)
{
    if (_members.isEmpty()) {
        populateMembers(engine, "xyzw");
        populateMembers(engine, "rgba");
        populateMembers(engine, "stpq");
    }
}

void VectorType::populateMembers(Engine *engine, const char *components)
{
    // Single component swizzles.
    for (int x = 0; x < _dimension; ++x) {
        const QString *name = engine->identifier(components + x, 1);
        add(engine->newVariable(this, *name, elementType()));
    }

    // Two component swizzles.
    const Type *vec2Type;
    if (_dimension == 2)
        vec2Type = this;
    else
        vec2Type = engine->vectorType(elementType(), 2);
    for (int x = 0; x < _dimension; ++x) {
        for (int y = 0; y < _dimension; ++y) {
            QString name;
            name += QLatin1Char(components[x]);
            name += QLatin1Char(components[y]);
            add(engine->newVariable
                    (this, *engine->identifier(name), vec2Type));
        }
    }

    // Three component swizzles.
    const Type *vec3Type;
    if (_dimension == 3)
        vec3Type = this;
    else if (_dimension < 3)
        return;
    else
        vec3Type = engine->vectorType(elementType(), 3);
    for (int x = 0; x < _dimension; ++x) {
        for (int y = 0; y < _dimension; ++y) {
            for (int z = 0; z < _dimension; ++z) {
                QString name;
                name += QLatin1Char(components[x]);
                name += QLatin1Char(components[y]);
                name += QLatin1Char(components[z]);
                add(engine->newVariable
                        (this, *engine->identifier(name), vec3Type));
            }
        }
    }

    // Four component swizzles.
    if (_dimension != 4)
        return;
    for (int x = 0; x < _dimension; ++x) {
        for (int y = 0; y < _dimension; ++y) {
            for (int z = 0; z < _dimension; ++z) {
                for (int w = 0; w < _dimension; ++w) {
                    QString name;
                    name += QLatin1Char(components[x]);
                    name += QLatin1Char(components[y]);
                    name += QLatin1Char(components[z]);
                    name += QLatin1Char(components[w]);
                    add(engine->newVariable
                            (this, *engine->identifier(name), this));
                }
            }
        }
    }
}

bool VectorType::isEqualTo(const Type *other) const
{
    if (other) {
        if (const VectorType *v = other->asVectorType()) {
            if (_dimension != v->dimension())
                return false;
            else if (elementType() != v->elementType())
                return false;
            return true;
        }
    }
    return false;
}

bool VectorType::isLessThan(const Type *other) const
{
    Q_ASSERT(other != 0);
    const VectorType *vec = other->asVectorType();
    Q_ASSERT(vec != 0);
    if (_dimension < vec->dimension())
        return true;
    else if (_dimension == vec->dimension() && elementType() < vec->elementType())
        return true;
    return false;
}

bool MatrixType::isEqualTo(const Type *other) const
{
    if (other) {
        if (const MatrixType *v = other->asMatrixType()) {
            if (_columns != v->columns())
                return false;
            else if (_rows != v->rows())
                return false;
            else if (_elementType != v->elementType())
                return false;
            return true;
        }
    }
    return false;
}

bool MatrixType::isLessThan(const Type *other) const
{
    Q_ASSERT(other != 0);
    const MatrixType *mat = other->asMatrixType();
    Q_ASSERT(mat != 0);
    if (_columns < mat->columns())
        return true;
    else if (_columns == mat->columns()) {
        if (_rows < mat->rows())
            return true;
        else if (_rows == mat->rows() && _elementType < mat->elementType())
            return true;
    }
    return false;
}

bool ArrayType::isEqualTo(const Type *other) const
{
    if (other) {
        if (const ArrayType *array = other->asArrayType())
            return elementType()->isEqualTo(array->elementType());
    }
    return false;
}

bool ArrayType::isLessThan(const Type *other) const
{
    Q_ASSERT(other != 0);
    const ArrayType *array = other->asArrayType();
    Q_ASSERT(array != 0);
    return elementType() < array->elementType();
}

void Struct::add(Symbol *member)
{
    _members.append(member);
}

Symbol *Struct::find(const QString &name) const
{
    foreach (Symbol *s, _members) {
        if (s->name() == name)
            return s;
    }
    return 0;
}

bool Struct::isEqualTo(const Type *other) const
{
    Q_UNUSED(other);
    return false;
}

bool Struct::isLessThan(const Type *other) const
{
    Q_UNUSED(other);
    return false;
}

const Type *Function::returnType() const
{
    return _returnType;
}

void Function::setReturnType(const Type *returnType)
{
    _returnType = returnType;
}

QVector<Argument *> Function::arguments() const
{
    return _arguments;
}

void Function::addArgument(Argument *arg)
{
    _arguments.append(arg);
}

int Function::argumentCount() const
{
    return _arguments.size();
}

Argument *Function::argumentAt(int index) const
{
    return _arguments.at(index);
}

bool Function::isEqualTo(const Type *other) const
{
    Q_UNUSED(other);
    return false;
}

bool Function::isLessThan(const Type *other) const
{
    Q_UNUSED(other);
    return false;
}

Symbol *Function::find(const QString &name) const
{
    foreach (Argument *arg, _arguments) {
        if (arg->name() == name)
            return arg;
    }
    return 0;
}

bool SamplerType::isEqualTo(const Type *other) const
{
    if (other) {
        if (const SamplerType *samp = other->asSamplerType())
            return _kind == samp->kind();
    }
    return false;
}

bool SamplerType::isLessThan(const Type *other) const
{
    Q_ASSERT(other != 0);
    const SamplerType *samp = other->asSamplerType();
    Q_ASSERT(samp != 0);
    return _kind < samp->kind();
}

OverloadSet::OverloadSet(Scope *enclosingScope)
    : Scope(enclosingScope)
{
}

QVector<Function *> OverloadSet::functions() const
{
    return _functions;
}

void OverloadSet::addFunction(Function *function)
{
    _functions.append(function);
}

const Type *OverloadSet::type() const
{
    return this;
}

Symbol *OverloadSet::find(const QString &) const
{
    return 0;
}

void OverloadSet::add(Symbol *symbol)
{
    if (symbol) {
        if (Function *fun = symbol->asFunction())
            addFunction(fun);
    }
}

bool OverloadSet::isEqualTo(const Type *other) const
{
    Q_UNUSED(other);
    return false;
}

bool OverloadSet::isLessThan(const Type *other) const
{
    Q_UNUSED(other);
    return false;
}