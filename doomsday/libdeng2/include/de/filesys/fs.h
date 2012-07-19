/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009, 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBDENG2_FS_H
#define LIBDENG2_FS_H

#include "../libdeng2.h"
#include "../Folder"

#include <map>

/**
 * @defgroup fs File System
 *
 * The file system (de::FS) governs a tree of files and folders, and provides the means to
 * access all data in libdeng2.
 */

namespace de
{
    namespace internal {
        template <typename Type>
        inline bool cannotCastFileTo(File* file) {
            return dynamic_cast<Type*>(file) == NULL;
        }
    }
        
    /**
     * The file system maintains a tree of files and folders. It provides a way
     * to quickly and efficiently locate files anywhere in the tree. It also
     * maintains semantic information about the structure and content of the 
     * file tree, allowing others to know how to treat the files and folders.
     *
     * In practice, the file system consists of a tree of File and Folder instances.
     * These instances are generated by the Feed objects attached to the folders.
     * For instance, a DirectoryFeed will generate the appropriate File and Folder
     * instances for a directory in the native file system. 
     *
     * The file system can be repopulated at any time to resynchronize it with the
     * source data. Repopulation is nondestructive as long as the source data has not
     * changed. Repopulation is needed for instance when native files get deleted 
     * in the directory a folder is feeding on. The feeds are responsible for
     * deciding when instances get out-of-date and need to be deleted (pruning). 
     * Pruning occurs when a folder that is already populated with files is repopulated.
     *
     * @ingroup fs
     */
    class DENG2_PUBLIC FS
    {
    public:
        /// No index is found for the specified type. @ingroup errors
        DENG2_ERROR(UnknownTypeError);
        
        /// No files found. @ingroup errors
        DENG2_ERROR(NotFoundError);
        
        /// More than one file found and there is not enough information to choose 
        /// between them. @ingroup errors
        DENG2_ERROR(AmbiguousError);
        
        typedef std::multimap<String, File*> Index;
        typedef std::pair<Index::iterator, Index::iterator> IndexRange;
        typedef std::pair<Index::const_iterator, Index::const_iterator> ConstIndexRange;
        typedef std::pair<String, File*> IndexEntry;
        typedef std::list<File*> FoundFiles;
        
    public:
        /**
         * Constructs a new file system. The file system needs to be manually
         * refreshed; initially it is empty.
         */
        FS();
        
        virtual ~FS();

        void printIndex();
        
        Folder& root();
        
        /**
         * Refresh the file system. Populates all folders with files from the feeds.
         */
        void refresh();
        
        /**
         * Retrieves a folder in the file system. The folder gets created if it
         * does not exist. Any missing parent folders will also be created.
         *
         * @param path  Path of the folder. Relative to the root folder.
         */
        Folder& makeFolder(const String& path);

        /**
         * Finds all files matching a full or partial path.
         *
         * @param path   Path or file name to look for.
         * @param found  Set of files that match the result.
         *
         * @return  Number of files found.
         */ 
        int findAll(const String& path, FoundFiles& found) const;
        
        /**
         * Finds a single file matching a full or partial path.
         *
         * @param path   Path or file name to look for.
         * 
         * @return  The found file.
         */
        File& find(const String& path) const;
        
        /**
         * Finds a file of a specific type.
         *
         * @param path  Full/partial path or file name to look for.
         */
        template <typename Type>
        Type& find(const String& path) const {
            FoundFiles found;
            findAll(path, found);
            // Filter out the wrong types.
            found.remove_if(internal::cannotCastFileTo<Type>);
            if(found.size() > 1) {
                /// @throw AmbiguousError  More than one file matches the conditions.
                throw AmbiguousError("FS::find", "More than one file found matching '" + path + "'");
            }
            if(found.empty()) {
                /// @throw NotFoundError  No files found matching the condition.
                throw NotFoundError("FS::find", "No files found matching '" + path + "'");
            }
            return *dynamic_cast<Type*>(found.front());
        }
        
        /**
         * Creates an interpreter for the data in a file. 
         *
         * @param sourceData  File with the source data. While interpreting, ownership
         *                    of the file is given to de::FS.
         *
         * @return  If the format of the source data was recognized, returns a new File
         *          that can be used for accessing the data. Ownership of the @a sourceData 
         *          will be transferred to the new interpreter File instance.
         *          If the format was not recognized, @a sourceData is returned as is
         *          and ownership is returned to the caller.
         */
        File* interpret(File* sourceData);
        
        /**
         * Provides access to the main index of the file system. This can be used for
         * efficiently looking up files based on name. @note The file names are
         * indexed in lower case.
         */
        const Index& nameIndex() const;
        
        /**
         * Retrieves the index of files of a particular type.
         *
         * @param typeIdentifier  Type identifier to look for. Use the TYPE_NAME() macro. 
         *
         * @return  A subset of the main index containing only the entries of the given type.
         *
         * For example, to look up the index for NativeFile instances:
         * @code
         * FS::Index& nativeFileIndex = App::fileSystem().indexFor(TYPE_NAME(NativeFile));
         * @endcode
         */
        const Index& indexFor(const String& typeIdentifier) const;
        
        /**
         * Adds a file to the main index.
         *
         * @param file  File to index.
         */
        void index(File& file);
        
        /**
         * Removes a file from the main index.
         *
         * @param file  File to remove from the index.
         */
        void deindex(File& file);

    private:  
        struct Instance;
        Instance* d;
    };
}

#endif /* LIBDENG2_FS_H */
