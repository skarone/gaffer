//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2012, John Haddon. All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are
//  met:
//
//      * Redistributions of source code must retain the above
//        copyright notice, this list of conditions and the following
//        disclaimer.
//
//      * Redistributions in binary form must reproduce the above
//        copyright notice, this list of conditions and the following
//        disclaimer in the documentation and/or other materials provided with
//        the distribution.
//
//      * Neither the name of John Haddon nor the names of
//        any other contributors to this software may be used to endorse or
//        promote products derived from this software without specific prior
//        written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
//  IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
//  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
//  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
//  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
//  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
//  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
//  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
//  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
//  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
//  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////

#include "boost/python.hpp"

#include "IECore/MessageHandler.h"
#include "IECorePython/RefCountedBinding.h"
#include "IECorePython/ScopedGILLock.h"

#include "Gaffer/Expression.h"
#include "Gaffer/StringPlug.h"

#include "GafferBindings/DependencyNodeBinding.h"
#include "GafferBindings/ExpressionBinding.h"
#include "GafferBindings/ExceptionAlgo.h"

using namespace boost::python;
using namespace GafferBindings;
using namespace Gaffer;

namespace
{

void setExpression( Expression &e, const std::string &expression, const std::string &language )
{
	IECorePython::ScopedGILRelease gilRelease;
	e.setExpression( expression, language );
}

tuple getExpression( Expression &e )
{
	std::string language;
	std::string expression = e.getExpression( language );
	return make_tuple( expression, language );
}

struct ExpressionEngineCreator
{
	ExpressionEngineCreator( object fn )
		:	m_fn( fn )
	{
	}

	Expression::EnginePtr operator()()
	{
		IECorePython::ScopedGILLock gilLock;
		Expression::EnginePtr result = extract<Expression::EnginePtr>( m_fn() );
		return result;
	}

	private :

		object m_fn;

};

class EngineWrapper : public IECorePython::RefCountedWrapper<Expression::Engine>
{
	public :

		EngineWrapper( PyObject *self )
				:	IECorePython::RefCountedWrapper<Expression::Engine>( self )
		{
		}

		virtual void parse( Expression *node, const std::string &expression, std::vector<ValuePlug *> &inputs, std::vector<ValuePlug *> &outputs, std::vector<IECore::InternedString> &contextVariables )
		{
			if( isSubclassed() )
			{
				IECorePython::ScopedGILLock gilLock;
				try
				{
					object f = this->methodOverride( "parse" );
					if( f )
					{
						list pythonInputs, pythonOutputs, pythonContextVariables;
						f( ExpressionPtr( node ), expression, pythonInputs, pythonOutputs, pythonContextVariables );

						container_utils::extend_container( inputs, pythonInputs );
						container_utils::extend_container( outputs, pythonOutputs );
						container_utils::extend_container( contextVariables, pythonContextVariables );
						return;
					}
				}
				catch( const error_already_set &e )
				{
					translatePythonException();
				}
			}

			throw IECore::Exception( "Engine::parse() python method not defined" );
		}

		virtual IECore::ConstObjectVectorPtr execute( const Context *context, const std::vector<const ValuePlug *> &proxyInputs ) const
		{
			if( isSubclassed() )
			{
				IECorePython::ScopedGILLock gilLock;
				try
				{
					object f = this->methodOverride( "execute" );
					if( f )
					{
						list pythonProxyInputs;
						for( std::vector<const ValuePlug *>::const_iterator it = proxyInputs.begin(); it!=proxyInputs.end(); it++ )
						{
							pythonProxyInputs.append( PlugPtr( const_cast<ValuePlug *>( *it ) ) );
						}

						object result = f( ContextPtr( const_cast<Context *>( context ) ), pythonProxyInputs );
						return extract<IECore::ConstObjectVectorPtr>( result );
					}
				}
				catch( const error_already_set &e )
				{
					translatePythonException();
				}
			}

			throw IECore::Exception( "Engine::execute() python method not defined" );
		}

		virtual void apply( ValuePlug *plug, const IECore::Object *value ) const
		{
			if( isSubclassed() )
			{
				IECorePython::ScopedGILLock gilLock;
				try
				{
					object f = this->methodOverride( "apply" );
					if( f )
					{
						f( ValuePlugPtr( plug ), IECore::ObjectPtr( const_cast<IECore::Object *>( value ) ) );
						return;
					}
				}
				catch( const error_already_set &e )
				{
					translatePythonException();
				}
			}

			throw IECore::Exception( "Engine::apply() python method not defined" );
		}

		static void registerEngine( const std::string &engineType, object creator )
		{
			Expression::Engine::registerEngine( engineType, ExpressionEngineCreator( creator ) );
		}

		static tuple registeredEngines()
		{
			std::vector<std::string> engineTypes;
			Expression::Engine::registeredEngines( engineTypes );
			boost::python::list l;
			for( std::vector<std::string>::const_iterator it = engineTypes.begin(); it!=engineTypes.end(); it++ )
			{
				l.append( *it );
			}
			return boost::python::tuple( l );
		}

};

static tuple languages()
{
	std::vector<std::string> languages;
	Expression::languages( languages );
	boost::python::list l;
	for( std::vector<std::string>::const_iterator it = languages.begin(); it!=languages.end(); it++ )
	{
		l.append( *it );
	}
	return boost::python::tuple( l );
}

} // namespace

void GafferBindings::bindExpression()
{

	scope s = DependencyNodeClass<Expression>()
		.def( "setExpression", &setExpression, ( arg( "expression" ), arg( "language" ) = "python" ) )
		.def( "getExpression", &getExpression )
		.def( "languages", &languages ).staticmethod( "languages" )
	;

	IECorePython::RefCountedClass<Expression::Engine, IECore::RefCounted, EngineWrapper>( "Engine" )
		.def( init<>() )
		.def( "registerEngine", &EngineWrapper::registerEngine ).staticmethod( "registerEngine" )
		.def( "registeredEngines", &EngineWrapper::registeredEngines ).staticmethod( "registeredEngines" )
	;

}
