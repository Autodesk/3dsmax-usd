#pragma once

#include "CoreExport.h"

#pragma warning(push)
#pragma warning(disable: 4127 4251 4275 4512 4800 )
#include <QtWidgets/QScrollArea>
#pragma warning(pop)


namespace MaxSDK
{
	class QmaxRollup; 
	class QmaxRollupContainerPrivate;
	class CoreExport QmaxRollupContainer : public QScrollArea
	{
		Q_OBJECT
	public:
		explicit QmaxRollupContainer( QWidget *parent = nullptr );
		
		virtual ~QmaxRollupContainer();

		void addRollup( QmaxRollup* rollup );
		void insertRollup( QmaxRollup* rollup, int index );

		int numRollups() const;
		QmaxRollup* rollup( int index ) const;
		QVector<QmaxRollup*> rollups() const; 
		int indexOf( QmaxRollup* rollup ) const; 

		/** \brief Resets the categories of the rollups back to the default state.
		 * Calls sortRollups() internally.
		 * \see sortRollups, QmaxRollup::category(), QmaxRollup::resetCategory()
		 */
		void resetCategories(); 

		QmaxRollup* takeRollup( int index );
		void takeRollup( QmaxRollup* rollup );

		/** \brief Sorts the rollups by category. 
		 * Sorts the rollups in based on ascending category. Rollups with the 
		 * same category will appear in the given order. 
		 * \see QmaxRollup.category()
		 **/
		void sortRollups(); 

		virtual void openAllRollups();
		virtual void closeAllRollups();

		/** \brief	Adds a QAction that get used within the right-click context 
		 *			menu of the rollup-container.
		 *	The rollup container does not take ownership of the QAction. To 
		 *	remove an action, call removeRightClickAction() or simply delete
		 *	the QAction on callers side. 
		 *	\see removeRightClickAction, QAction
		 */
		virtual void addRightClickAction( QAction* action );
		
		/** \brief	Remove a QAction that from used within the right-click 
		*			context menu of the rollup-container.
		*	\see addRightClickAction, QAction
		*/
		virtual void removeRightClickAction( QAction* action );

		QSize minimumSizeHint() const override;
        
		bool canScroll() const;

	signals:

	public slots:

	protected:
		
		explicit QmaxRollupContainer( QmaxRollupContainerPrivate* d, QWidget *parent = nullptr );

		void mouseMoveEvent( QMouseEvent *event ) override;
		void mousePressEvent( QMouseEvent *event ) override;
		void mouseReleaseEvent( QMouseEvent *event ) override;
		void wheelEvent( QWheelEvent *event ) override; 

		void contextMenuEvent( QContextMenuEvent *event ) override;

		void dragEnterEvent( QDragEnterEvent *event ) override; 
		void dragMoveEvent( QDragMoveEvent *event ) override;
		void dragLeaveEvent( QDragLeaveEvent *event ) override; 
		void dropEvent( QDropEvent* event ) override; 

		Q_DECLARE_PRIVATE( QmaxRollupContainer );

	private:
		QmaxRollupContainerPrivate* d_ptr;
		Q_DISABLE_COPY( QmaxRollupContainer );

		friend class QmaxRollup; 
	};

};