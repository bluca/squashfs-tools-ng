/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * rbtree.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "rbtree.h"
#include "test.h"

static int key_compare(const void *a, const void *b)
{
	return *((sqfs_s32 *)a) - *((sqfs_s32 *)b);
}

static size_t count_nodes_dfs(rbtree_node_t *n)
{
	return 1 + (n->left == NULL ? 0 : count_nodes_dfs(n->left))
		+ (n->right == NULL ? 0 : count_nodes_dfs(n->right));
}

static size_t min_depth(rbtree_node_t *n)
{
	size_t lhs, rhs;

	if (n == NULL)
		return 0;

	lhs = min_depth(n->left) + 1;
	rhs = min_depth(n->right) + 1;

	return lhs < rhs ? lhs : rhs;
}

static size_t max_depth(rbtree_node_t *n)
{
	size_t lhs, rhs;

	if (n == NULL)
		return 0;

	lhs = min_depth(n->left) + 1;
	rhs = min_depth(n->right) + 1;

	return lhs > rhs ? lhs : rhs;
}

static size_t get_ref_black_depth(rbtree_t *rb)
{
	rbtree_node_t *n;
	size_t count = 0;

	for (n = rb->root; n != NULL; n = n->left) {
		if (!n->is_red)
			count += 1;
	}

	return count;
}

static void check_binary_tree_dfs(rbtree_node_t *n)
{
	const void *key = rbtree_node_key(n);
	const void *cmp;

	if (n->left != NULL) {
		cmp = rbtree_node_key(n->left);
		TEST_ASSERT(key_compare(cmp, key) < 0);

		check_binary_tree_dfs(n->left);
	}

	if (n->right != NULL) {
		cmp = rbtree_node_key(n->right);
		TEST_ASSERT(key_compare(cmp, key) > 0);

		check_binary_tree_dfs(n->right);
	}
}

static void check_colors_dfs(rbtree_node_t *n)
{
	if (n->is_red) {
		TEST_ASSERT(n->left == NULL || !n->left->is_red);
		TEST_ASSERT(n->right == NULL || !n->right->is_red);
	}

	if (n->left != NULL)
		check_colors_dfs(n->left);

	if (n->right != NULL)
		check_colors_dfs(n->right);
}

static void check_black_depth_dfs(rbtree_node_t *n, size_t ref,
				  size_t counter)
{
	if (!n->is_red)
		counter += 1;

	if (n->left == NULL || n->right == NULL)
		TEST_EQUAL_UI(counter, ref);

	if (n->left != NULL)
		check_black_depth_dfs(n->left, ref, counter);

	if (n->right != NULL)
		check_black_depth_dfs(n->right, ref, counter);
}

int main(void)
{
	size_t count, blkdepth, mind, maxd;
	sqfs_s32 key, key2;
	rbtree_node_t *n;
	sqfs_u64 value;
	rbtree_t rb;

	TEST_ASSERT(rbtree_init(&rb, sizeof(sqfs_s32),
				sizeof(sqfs_u64), key_compare) == 0);

	count = 0;

	for (key = -1000; key < 1000; ++key) {
		/* lookup of current key must fail prior to insert */
		TEST_NULL(rbtree_lookup(&rb, &key));

		/* previous key/value pairs must still be there */
		for (key2 = -1000; key2 < key; ++key2) {
			n = rbtree_lookup(&rb, &key2);
			TEST_NOT_NULL(n);
			value = *((sqfs_u64 *)rbtree_node_value(n));
			TEST_EQUAL_UI((sqfs_u64)(key2 + 10000), value);
		}

		/* insert key value pair */
		value = key + 10000;
		TEST_ASSERT(rbtree_insert(&rb, &key, &value) == 0);
		count += 1;

		/* check if the tree has the right number of nodes */
		TEST_EQUAL_UI(count_nodes_dfs(rb.root), count);

		/* check if it is still a binary tree */
		check_binary_tree_dfs(rb.root);

		/* root node must be black. Every red node
		   must have black children. */
		TEST_ASSERT(!rb.root->is_red);
		check_colors_dfs(rb.root);

		/* every path from the root to a leave must have
		   the same number of black nodes. */
		blkdepth = get_ref_black_depth(&rb);
		check_black_depth_dfs(rb.root, blkdepth, 0);

		/* longest root to leaf path must be at most
		   twice as long as the shortest. */
		mind = min_depth(rb.root);
		maxd = max_depth(rb.root);
		TEST_ASSERT(maxd <= mind * 2);

		/* lookup of current key must work after insert */
		n = rbtree_lookup(&rb, &key);
		TEST_NOT_NULL(n);
		value = *((sqfs_u64 *)rbtree_node_value(n));
		TEST_EQUAL_UI((sqfs_u64)(key + 10000), value);
	}

	rbtree_cleanup(&rb);
	return EXIT_SUCCESS;
}
